# IOT-73 — exposing sensors: the registry joined to each sensor's latest reading.

from datetime import datetime, timezone

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app import main as main_module
from app.db import Base, get_session
from app.main import app
from app.models import Device, Location, Sensor

NOW = datetime(2026, 9, 7, 12, 0, 0, tzinfo=timezone.utc)


class FakeActivity:
    # Stands in for LedgerActivity; the derivation is covered in test_activity.py.
    def __init__(self, latest: dict | None = None, reachable: bool = True,
                 cameras: dict | None = None) -> None:
        self._latest = latest or {}
        self._cameras = cameras or {}
        self.reachable = reachable

    async def refresh(self, client=None) -> None:
        return None

    def last_seen(self, device_id: str):
        return None

    def status(self, device_id: str, now=None) -> str:
        return "inactive"

    def latest_reading(self, device_id: str, sensor_id: str):
        return self._latest.get((device_id, sensor_id))

    def sensors_seen(self, device_id: str) -> set[str]:
        return {sid for (did, sid) in self._latest if did == device_id}

    def camera_state(self, device_id: str, sensor_id: str):
        return self._cameras.get((device_id, sensor_id))


def reading(sensor_id: str, value, unit: str, stype: str = "temperature") -> dict:
    return {
        "sensor_id": sensor_id,
        "sensor_type": stype,
        "value": value,
        "unit": unit,
        "seq": 1,
        "at": NOW,
    }


@pytest.fixture()
def client():
    engine = create_engine(
        "sqlite://", connect_args={"check_same_thread": False}, poolclass=StaticPool
    )
    Base.metadata.create_all(bind=engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)

    def override_session():
        session = factory()
        try:
            yield session
        finally:
            session.close()

    app.dependency_overrides[get_session] = override_session
    with factory() as seed:
        seed.add(Location(id="warehouse-a", name="Warehouse A"))
        seed.add(Device(device_id="esp32-01", location_id="warehouse-a", device_type="ESP32"))
        seed.add(Device(device_id="bare-board", device_type="ESP32"))
        seed.add(Sensor(device_id="esp32-01", sensor_id="temp-0",
                        sensor_type="temperature", unit="celsius"))
        seed.add(Sensor(device_id="esp32-01", sensor_id="temp-1",
                        sensor_type="temperature", unit="celsius"))
        seed.add(Sensor(device_id="esp32-01", sensor_id="cam-0",
                        sensor_type="camera", unit="stream"))
        seed.commit()

    yield TestClient(app)
    app.dependency_overrides.clear()
    engine.dispose()


def test_lists_a_devices_sensors(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity())
    body = client.get("/devices/esp32-01/sensors").json()

    assert [s["sensor_id"] for s in body["sensors"]] == ["cam-0", "temp-0", "temp-1"]
    assert body["count"] == 3
    assert all(s["registered"] for s in body["sensors"])


def test_two_sensors_of_one_type_on_a_board(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity())
    body = client.get("/devices/esp32-01/sensors").json()

    temps = [s for s in body["sensors"] if s["sensor_type"] == "temperature"]
    assert {s["sensor_id"] for s in temps} == {"temp-0", "temp-1"}


def test_includes_the_latest_reading_per_sensor(client, monkeypatch) -> None:
    # The panel needs this on first paint, without pulling a whole series.
    monkeypatch.setattr(
        main_module,
        "ledger_activity",
        FakeActivity({("esp32-01", "temp-0"): reading("temp-0", 21.4, "celsius")}),
    )
    body = client.get("/devices/esp32-01/sensors").json()
    by_id = {s["sensor_id"]: s for s in body["sensors"]}

    assert by_id["temp-0"]["latest"]["value"] == 21.4
    assert by_id["temp-0"]["latest"]["unit"] == "celsius"
    # Registered but never heard from.
    assert by_id["temp-1"]["latest"] is None


def test_a_failed_reading_is_null_not_missing(client, monkeypatch) -> None:
    # ADR 0010: null means the sensor failed, and must survive to the client.
    monkeypatch.setattr(
        main_module,
        "ledger_activity",
        FakeActivity({("esp32-01", "temp-0"): reading("temp-0", None, "celsius")}),
    )
    body = client.get("/devices/esp32-01/sensors").json()
    latest = next(s for s in body["sensors"] if s["sensor_id"] == "temp-0")["latest"]

    assert latest is not None
    assert latest["value"] is None


def test_a_vector_reading_survives_unchanged(client, monkeypatch) -> None:
    monkeypatch.setattr(
        main_module,
        "ledger_activity",
        FakeActivity({("esp32-01", "temp-0"): reading("temp-0", [0.1, 0.2, 9.8], "m_s2",
                                                      "accelerometer")}),
    )
    body = client.get("/devices/esp32-01/sensors").json()
    latest = next(s for s in body["sensors"] if s["sensor_id"] == "temp-0")["latest"]

    assert latest["value"] == [0.1, 0.2, 9.8]


def test_unregistered_sensors_are_reported_not_hidden(client, monkeypatch) -> None:
    # A typo in a firmware config produces readings belonging to no registered
    # sensor. Surfacing it is how that typo gets found.
    monkeypatch.setattr(
        main_module,
        "ledger_activity",
        FakeActivity({("esp32-01", "tpm-0"): reading("tpm-0", 21.0, "celsius")}),
    )
    body = client.get("/devices/esp32-01/sensors").json()
    by_id = {s["sensor_id"]: s for s in body["sensors"]}

    assert "tpm-0" in by_id
    assert by_id["tpm-0"]["registered"] is False
    assert by_id["tpm-0"]["latest"]["value"] == 21.0
    assert body["unregistered_count"] == 1


def test_a_device_with_no_sensors_is_empty_not_an_error(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity())
    response = client.get("/devices/bare-board/sensors")

    assert response.status_code == 200
    assert response.json()["sensors"] == []


def test_an_unknown_device_is_404(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity())
    response = client.get("/devices/no-such-board/sensors")

    assert response.status_code == 404
    assert "no-such-board" in response.json()["detail"]


def test_an_unreachable_ledger_still_lists_the_registry(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity(reachable=False))
    body = client.get("/devices/esp32-01/sensors").json()

    assert body["ledger_reachable"] is False
    assert len(body["sensors"]) == 3


def test_devices_list_carries_a_sensor_summary(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity())
    body = client.get("/devices").json()
    by_id = {d["device_id"]: d for d in body["devices"]}

    assert by_id["esp32-01"]["sensor_count"] == 3
    assert by_id["esp32-01"]["sensor_types"] == ["camera", "temperature"]
    assert by_id["bare-board"]["sensor_count"] == 0
    assert by_id["bare-board"]["sensor_types"] == []

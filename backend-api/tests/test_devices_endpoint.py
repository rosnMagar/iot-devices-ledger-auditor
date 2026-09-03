# IOT-35 — GET /devices: the registry joined to derived activity.

from datetime import datetime, timedelta, timezone

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app import main as main_module
from app.db import Base, get_session
from app.main import app
from app.models import Device, Location

NOW = datetime(2026, 9, 3, 12, 0, 0, tzinfo=timezone.utc)


class FakeActivity:
    # Stands in for LedgerActivity; the derivation is covered in test_activity.py.

    def __init__(self, last_seen: dict[str, datetime], reachable: bool = True) -> None:
        self._last_seen = last_seen
        self.reachable = reachable
        self.refreshed = 0

    async def refresh(self, client=None) -> None:
        self.refreshed += 1

    def last_seen(self, device_id: str):
        return self._last_seen.get(device_id)

    def status(self, device_id: str, now=None) -> str:
        return "active" if device_id in self._last_seen else "inactive"


@pytest.fixture()
def client(monkeypatch):
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
        seed.add(Location(id="loc-1", name="Warehouse A"))
        seed.add(Device(device_id="esp32-01", location_id="loc-1", device_type="DHT22"))
        seed.add(Device(device_id="esp32-02", location_id="loc-1", device_type="DHT22"))
        seed.commit()

    # Not `with TestClient(app)`: the lifespan calls init_db() against the real
    # DATABASE_URL and leaves a stray app.db. The session is overridden anyway.
    yield TestClient(app)

    app.dependency_overrides.clear()
    engine.dispose()


def test_devices_lists_the_registry_with_derived_status(client, monkeypatch) -> None:
    monkeypatch.setattr(
        main_module,
        "ledger_activity",
        FakeActivity({"esp32-01": NOW - timedelta(seconds=30)}),
    )

    response = client.get("/devices")
    assert response.status_code == 200
    body = response.json()

    ids = [d["device_id"] for d in body["devices"]]
    assert ids == ["esp32-01", "esp32-02"]

    first, second = body["devices"]
    assert first["status"] == "active"
    assert first["last_seen"] is not None
    assert first["device_type"] == "DHT22"
    assert first["location_id"] == "loc-1"

    # Never reported: inactive, with a null last_seen.
    assert second["status"] == "inactive"
    assert second["last_seen"] is None


def test_devices_reports_the_configured_window(client, monkeypatch) -> None:
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity({}))
    body = client.get("/devices").json()
    assert isinstance(body["active_window_seconds"], int)


def test_devices_refreshes_activity_on_each_request(client, monkeypatch) -> None:
    fake = FakeActivity({})
    monkeypatch.setattr(main_module, "ledger_activity", fake)

    client.get("/devices")
    client.get("/devices")

    # Otherwise a device could read active minutes after it stopped reporting.
    assert fake.refreshed == 2


def test_an_unreachable_ledger_still_serves_the_registry(client, monkeypatch) -> None:
    monkeypatch.setattr(
        main_module, "ledger_activity", FakeActivity({}, reachable=False)
    )

    response = client.get("/devices")
    assert response.status_code == 200
    body = response.json()

    # Registry still correct, but the staleness is reported rather than hidden.
    assert body["ledger_reachable"] is False
    assert len(body["devices"]) == 2


def test_devices_is_empty_when_nothing_is_registered(monkeypatch) -> None:
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
    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity({}))

    body = TestClient(app).get("/devices").json()

    assert body["devices"] == []
    app.dependency_overrides.clear()
    engine.dispose()

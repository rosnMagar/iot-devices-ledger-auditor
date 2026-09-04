# IOT-36 — filters and sorting on GET /devices.

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
    def __init__(self, last_seen: dict[str, datetime]) -> None:
        self._last_seen = last_seen
        self.reachable = True

    async def refresh(self, client=None) -> None:
        pass

    def last_seen(self, device_id: str):
        return self._last_seen.get(device_id)

    def status(self, device_id: str, now=None) -> str:
        seen = self._last_seen.get(device_id)
        if seen is None:
            return "inactive"
        return "active" if NOW - seen <= timedelta(seconds=300) else "inactive"


# alpha: active, warehouse, DHT22   | bravo: stale, warehouse, BME280
# charlie: active, freezer, DHT22   | delta: never seen, no location, DHT22
FLEET = {
    "alpha": NOW - timedelta(seconds=30),
    "bravo": NOW - timedelta(hours=3),
    "charlie": NOW - timedelta(seconds=120),
}


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
        seed.add(Location(id="warehouse", name="Warehouse A"))
        seed.add(Location(id="freezer", name="Freezer"))
        seed.add(Device(device_id="alpha", location_id="warehouse", device_type="DHT22"))
        seed.add(Device(device_id="bravo", location_id="warehouse", device_type="BME280"))
        seed.add(Device(device_id="charlie", location_id="freezer", device_type="DHT22"))
        seed.add(Device(device_id="delta", device_type="DHT22"))
        seed.commit()

    monkeypatch.setattr(main_module, "ledger_activity", FakeActivity(FLEET))
    yield TestClient(app)

    app.dependency_overrides.clear()
    engine.dispose()


def ids(response) -> list[str]:
    return [d["device_id"] for d in response.json()["devices"]]


def test_defaults_are_all_by_last_seen_desc(client) -> None:
    response = client.get("/devices")
    assert response.status_code == 200
    body = response.json()

    # Most recently seen first; delta never reported so it sorts last.
    assert ids(response) == ["alpha", "charlie", "bravo", "delta"]
    assert body["sort"] == "last_seen"
    assert body["order"] == "desc"
    assert body["filters"]["status"] == "all"
    assert body["count"] == 4


def test_filter_by_status(client) -> None:
    assert ids(client.get("/devices?status=active")) == ["alpha", "charlie"]
    assert ids(client.get("/devices?status=inactive")) == ["bravo", "delta"]
    assert len(ids(client.get("/devices?status=all"))) == 4


def test_filter_by_location(client) -> None:
    assert ids(client.get("/devices?location_id=warehouse")) == ["alpha", "bravo"]
    assert ids(client.get("/devices?location_id=freezer")) == ["charlie"]
    assert ids(client.get("/devices?location_id=nowhere")) == []


def test_filter_by_device_type(client) -> None:
    assert ids(client.get("/devices?device_type=DHT22")) == [
        "alpha",
        "charlie",
        "delta",
    ]
    assert ids(client.get("/devices?device_type=BME280")) == ["bravo"]


def test_filters_combine(client) -> None:
    response = client.get("/devices?status=active&location_id=warehouse&device_type=DHT22")
    assert ids(response) == ["alpha"]
    assert response.json()["count"] == 1


def test_sort_by_name(client) -> None:
    assert ids(client.get("/devices?sort=name&order=asc")) == [
        "alpha",
        "bravo",
        "charlie",
        "delta",
    ]
    assert ids(client.get("/devices?sort=name&order=desc")) == [
        "delta",
        "charlie",
        "bravo",
        "alpha",
    ]


def test_sort_by_location(client) -> None:
    # delta has no location; it sorts as empty string, so first ascending.
    assert ids(client.get("/devices?sort=location&order=asc"))[0] == "delta"
    assert ids(client.get("/devices?sort=location&order=desc"))[-1] == "delta"


def test_sort_by_last_seen_ascending_puts_never_seen_first(client) -> None:
    # Never-reported sorts as the oldest possible time, so asc puts it first and
    # the default desc puts it last.
    assert ids(client.get("/devices?sort=last_seen&order=asc")) == [
        "delta",
        "bravo",
        "charlie",
        "alpha",
    ]


def test_sort_is_deterministic_when_values_tie(client) -> None:
    # Every device is DHT22-or-not but location ties; device_id breaks it.
    first = ids(client.get("/devices?sort=location&order=asc"))
    second = ids(client.get("/devices?sort=location&order=asc"))
    assert first == second


@pytest.mark.parametrize(
    ("query", "param"),
    [
        ("status=online", "status"),
        ("sort=whenever", "sort"),
        ("order=sideways", "order"),
    ],
)
def test_invalid_values_are_400_with_a_clear_message(client, query, param) -> None:
    response = client.get(f"/devices?{query}")
    assert response.status_code == 400
    detail = response.json()["detail"]
    assert param in detail
    # The message must name the allowed values, not just say "invalid".
    assert "must be one of" in detail


def test_unknown_query_params_are_ignored(client) -> None:
    # FastAPI ignores extras; asserted so a typo'd filter fails open (all
    # devices) rather than 500ing.
    assert client.get("/devices?colour=blue").status_code == 200


def test_filters_are_echoed_back(client) -> None:
    body = client.get("/devices?status=active&location_id=warehouse").json()
    assert body["filters"] == {
        "status": "active",
        "location_id": "warehouse",
        "device_type": None,
    }

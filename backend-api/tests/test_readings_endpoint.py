# IOT-62 — GET /readings: the history a chart draws before the live feed arrives.

from datetime import datetime, timedelta, timezone
from urllib.parse import quote

import httpx
import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app import activity as activity_module
from app import main as main_module
from app.activity import LedgerActivity
from app.db import Base, get_session
from app.main import app
from app.models import Device, Sensor

NOW = datetime(2026, 9, 7, 12, 0, 0, tzinfo=timezone.utc)


def block(index: int, actor: str, sensor_id: str, value, when: datetime,
          stype: str = "temperature", unit: str = "celsius",
          event_type: str = "SENSOR_READING") -> dict:
    return {
        "index": index,
        "timestamp": when.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "event": {
            "event_type": event_type,
            "location_id": "warehouse-a",
            "actor": actor,
            "description": "reading",
            "metadata": {
                "sensor_id": sensor_id,
                "sensor_type": stype,
                "value": value,
                "unit": unit,
                "seq": index,
            },
        },
        "prev_hash": "0" * 64,
        "hash": "a" * 64,
    }


def transport(blocks: list[dict]) -> httpx.MockTransport:
    def handler(request: httpx.Request) -> httpx.Response:
        return httpx.Response(200, json={"blocks": blocks, "chain_length": len(blocks)})

    return httpx.MockTransport(handler)


async def load(cache: LedgerActivity, blocks: list[dict]) -> None:
    async with httpx.AsyncClient(transport=transport(blocks)) as client:
        await cache.refresh(client)


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
        seed.add(Device(device_id="esp32-01", device_type="ESP32"))
        seed.add(Sensor(device_id="esp32-01", sensor_id="temp-0",
                        sensor_type="temperature", unit="celsius"))
        seed.commit()

    yield TestClient(app)
    app.dependency_overrides.clear()
    engine.dispose()


@pytest.fixture()
def loaded(client, monkeypatch):
    """A cache primed from a stub ledger, wired into the app."""
    async def _load(blocks):
        cache = LedgerActivity()
        await load(cache, blocks)

        class Frozen:
            reachable = cache.reachable
            readings = cache.readings

            async def refresh(self, http_client=None):
                return None

        monkeypatch.setattr(main_module, "ledger_activity", Frozen())
        return client

    return _load


@pytest.mark.asyncio()
async def test_returns_a_series_oldest_first(loaded) -> None:
    c = await loaded([
        block(1, "esp32-01", "temp-0", 21.0, NOW - timedelta(minutes=3)),
        block(2, "esp32-01", "temp-0", 21.5, NOW - timedelta(minutes=2)),
        block(3, "esp32-01", "temp-0", 22.0, NOW - timedelta(minutes=1)),
    ])
    body = c.get("/readings?device_id=esp32-01&sensor_id=temp-0").json()

    assert [r["value"] for r in body["series"]] == [21.0, 21.5, 22.0]
    assert body["count"] == 3


@pytest.mark.asyncio()
async def test_filters_by_sensor(loaded) -> None:
    c = await loaded([
        block(1, "esp32-01", "temp-0", 21.0, NOW - timedelta(minutes=2)),
        block(2, "esp32-01", "hum-0", 44.0, NOW - timedelta(minutes=1),
              "humidity", "percent"),
    ])
    body = c.get("/readings?device_id=esp32-01&sensor_id=temp-0").json()

    assert {r["sensor_id"] for r in body["series"]} == {"temp-0"}


@pytest.mark.asyncio()
async def test_without_a_sensor_id_returns_the_whole_device(loaded) -> None:
    # One request can back a whole panel.
    c = await loaded([
        block(1, "esp32-01", "temp-0", 21.0, NOW - timedelta(minutes=2)),
        block(2, "esp32-01", "hum-0", 44.0, NOW - timedelta(minutes=1),
              "humidity", "percent"),
    ])
    body = c.get("/readings?device_id=esp32-01").json()

    assert {r["sensor_id"] for r in body["series"]} == {"temp-0", "hum-0"}
    # Interleaved by time, not grouped by sensor.
    assert [r["at"] for r in body["series"]] == sorted(r["at"] for r in body["series"])


@pytest.mark.asyncio()
async def test_since_trims_the_head(loaded) -> None:
    c = await loaded([
        block(1, "esp32-01", "temp-0", 21.0, NOW - timedelta(minutes=10)),
        block(2, "esp32-01", "temp-0", 22.0, NOW - timedelta(minutes=1)),
    ])
    cutoff = quote((NOW - timedelta(minutes=5)).isoformat())
    body = c.get(f"/readings?device_id=esp32-01&since={cutoff}").json()

    assert [r["value"] for r in body["series"]] == [22.0]


@pytest.mark.asyncio()
async def test_an_unencoded_offset_still_works(loaded) -> None:
    # A "+" in a query string decodes to a space, so a correct ISO timestamp
    # arrives with its offset mangled. Returning 400 for a value the caller got
    # right is a confusing failure, so that one shape is repaired.
    c = await loaded([
        block(1, "esp32-01", "temp-0", 21.0, NOW - timedelta(minutes=10)),
        block(2, "esp32-01", "temp-0", 22.0, NOW - timedelta(minutes=1)),
    ])
    mangled = (NOW - timedelta(minutes=5)).isoformat().replace("+", " ")
    body = c.get("/readings", params={"device_id": "esp32-01", "since": mangled}).json()

    assert [r["value"] for r in body["series"]] == [22.0]


@pytest.mark.asyncio()
async def test_a_null_reading_survives_to_the_client(loaded) -> None:
    # ADR 0010: null means the sensor failed. A chart breaks the line rather
    # than plotting zero, so it has to arrive intact.
    c = await loaded([block(1, "esp32-01", "temp-0", None, NOW)])
    body = c.get("/readings?device_id=esp32-01").json()

    assert body["series"][0]["value"] is None


@pytest.mark.asyncio()
async def test_a_vector_reading_survives_unchanged(loaded) -> None:
    c = await loaded([
        block(1, "esp32-01", "acc-0", [0.1, -0.2, 9.8], NOW, "accelerometer", "m_s2")
    ])
    body = c.get("/readings?device_id=esp32-01").json()

    assert body["series"][0]["value"] == [0.1, -0.2, 9.8]


@pytest.mark.asyncio()
async def test_camera_events_are_not_readings(loaded) -> None:
    # ADR 0011: a CAMERA_EVENT has no value; it must not enter a chart.
    c = await loaded([
        block(1, "esp32-01", "cam-0", None, NOW, "camera", "stream",
              event_type="CAMERA_EVENT"),
        block(2, "esp32-01", "temp-0", 21.0, NOW),
    ])
    body = c.get("/readings?device_id=esp32-01").json()

    assert [r["sensor_id"] for r in body["series"]] == ["temp-0"]


@pytest.mark.asyncio()
async def test_malformed_blocks_are_skipped_not_fatal(loaded) -> None:
    bad_metadata = block(1, "esp32-01", "temp-0", 21.0, NOW)
    bad_metadata["event"]["metadata"] = "not-an-object"
    no_sensor = block(2, "esp32-01", "temp-0", 21.0, NOW)
    del no_sensor["event"]["metadata"]["sensor_id"]
    bad_time = block(3, "esp32-01", "temp-0", 21.0, NOW)
    bad_time["timestamp"] = "not-a-timestamp"
    good = block(4, "esp32-01", "temp-0", 23.0, NOW)

    c = await loaded([bad_metadata, no_sensor, bad_time, good])
    body = c.get("/readings?device_id=esp32-01").json()

    assert [r["value"] for r in body["series"]] == [23.0]


@pytest.mark.asyncio()
async def test_the_response_is_bounded_and_says_so(loaded) -> None:
    c = await loaded([
        block(i, "esp32-01", "temp-0", float(i), NOW - timedelta(seconds=100 - i))
        for i in range(1, 51)
    ])
    body = c.get("/readings?device_id=esp32-01&limit=10").json()

    assert body["count"] == 10
    assert body["truncated"] is True
    # Trimmed from the newest end: a chart wants the recent tail.
    assert [r["value"] for r in body["series"]] == [float(i) for i in range(41, 51)]


@pytest.mark.asyncio()
async def test_history_is_capped_per_sensor(monkeypatch) -> None:
    # The ledger grows without limit; this cache must not.
    monkeypatch.setattr(activity_module, "HISTORY_LIMIT", 5)
    cache = LedgerActivity()
    await load(cache, [
        block(i, "esp32-01", "temp-0", float(i), NOW - timedelta(seconds=100 - i))
        for i in range(1, 21)
    ])

    kept = cache.readings("esp32-01", "temp-0")
    assert len(kept) == 5
    assert [r["value"] for r in kept] == [16.0, 17.0, 18.0, 19.0, 20.0]


def test_an_unknown_device_is_404(client) -> None:
    response = client.get("/readings?device_id=nope")
    assert response.status_code == 404
    assert "nope" in response.json()["detail"]


@pytest.mark.parametrize("query", ["limit=0", "limit=-5", "limit=100000"])
def test_an_out_of_range_limit_is_400(client, query) -> None:
    response = client.get(f"/readings?device_id=esp32-01&{query}")
    assert response.status_code == 400
    assert "limit" in response.json()["detail"]


def test_a_malformed_since_is_400(client) -> None:
    response = client.get("/readings?device_id=esp32-01&since=yesterday")
    assert response.status_code == 400
    assert "ISO 8601" in response.json()["detail"]


def test_a_naive_since_is_treated_as_utc(client, monkeypatch) -> None:
    # Otherwise comparing naive against aware raises a TypeError mid-request.
    class Empty:
        reachable = True

        async def refresh(self, http_client=None):
            return None

        def readings(self, *a, **k):
            return []

    monkeypatch.setattr(main_module, "ledger_activity", Empty())
    response = client.get("/readings?device_id=esp32-01&since=2026-09-07T00:00:00")
    assert response.status_code == 200

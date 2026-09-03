# IOT-35 — deriving device activity from the ledger.

from datetime import datetime, timedelta, timezone

import httpx
import pytest

from app import activity as activity_module
from app.activity import LedgerActivity

NOW = datetime(2026, 9, 3, 12, 0, 0, tzinfo=timezone.utc)


def block(index: int, actor: str, when: datetime) -> dict:
    return {
        "index": index,
        "timestamp": when.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "event": {
            "event_type": "SENSOR_READING",
            "location_id": "loc-1",
            "actor": actor,
            "description": "reading",
            "metadata": {},
        },
        "prev_hash": "0" * 64,
        "hash": "a" * 64,
    }


def ledger_transport(pages: list[dict]) -> httpx.MockTransport:
    # One /blocks response per call; records each `from` param.
    calls: list[str | None] = []
    remaining = list(pages)

    def handler(request: httpx.Request) -> httpx.Response:
        calls.append(request.url.params.get("from"))
        payload = remaining.pop(0) if remaining else {"blocks": [], "chain_length": 0}
        return httpx.Response(200, json=payload)

    transport = httpx.MockTransport(handler)
    transport.calls = calls  # type: ignore[attr-defined]
    return transport


@pytest.fixture()
def window_300(monkeypatch):
    monkeypatch.setattr(activity_module, "ACTIVE_WINDOW_SECONDS", 300)


async def refresh_with(cache: LedgerActivity, transport: httpx.MockTransport) -> None:
    async with httpx.AsyncClient(transport=transport) as client:
        await cache.refresh(client)


@pytest.mark.asyncio()
async def test_last_seen_comes_from_the_most_recent_matching_block(window_300) -> None:
    cache = LedgerActivity("http://storage-core:8080")
    transport = ledger_transport(
        [
            {
                "blocks": [
                    block(1, "esp32-01", NOW - timedelta(minutes=10)),
                    block(2, "esp32-01", NOW - timedelta(minutes=1)),
                    block(3, "esp32-02", NOW - timedelta(hours=2)),
                ],
                "chain_length": 4,
            }
        ]
    )
    await refresh_with(cache, transport)

    assert cache.last_seen("esp32-01") == NOW - timedelta(minutes=1)
    assert cache.last_seen("esp32-02") == NOW - timedelta(hours=2)
    assert cache.last_seen("never-heard-of") is None


@pytest.mark.asyncio()
async def test_status_reflects_the_active_window(window_300) -> None:
    cache = LedgerActivity()
    transport = ledger_transport(
        [
            {
                "blocks": [
                    block(1, "recent", NOW - timedelta(seconds=60)),
                    block(2, "stale", NOW - timedelta(seconds=600)),
                    block(3, "edge", NOW - timedelta(seconds=300)),
                ],
                "chain_length": 4,
            }
        ]
    )
    await refresh_with(cache, transport)

    assert cache.status("recent", now=NOW) == "active"
    assert cache.status("stale", now=NOW) == "inactive"
    assert cache.status("edge", now=NOW) == "active"


@pytest.mark.asyncio()
async def test_a_device_that_never_reported_is_inactive_with_null_last_seen(
    window_300,
) -> None:
    cache = LedgerActivity()
    await refresh_with(cache, ledger_transport([{"blocks": [], "chain_length": 1}]))

    # Two states; the null separates "never showed up" from "went quiet".
    assert cache.status("esp32-unseen", now=NOW) == "inactive"
    assert cache.last_seen("esp32-unseen") is None


@pytest.mark.asyncio()
async def test_refresh_resumes_from_the_last_index_instead_of_rescanning() -> None:
    cache = LedgerActivity()
    transport = ledger_transport(
        [
            {"blocks": [block(1, "esp32-01", NOW - timedelta(minutes=5))], "chain_length": 2},
            {"blocks": [block(2, "esp32-01", NOW - timedelta(minutes=1))], "chain_length": 3},
        ]
    )

    await refresh_with(cache, transport)
    await refresh_with(cache, transport)

    # The point of the cache: the second call asks for the tail, not the start.
    assert transport.calls == ["0", "2"]
    assert cache.last_seen("esp32-01") == NOW - timedelta(minutes=1)


@pytest.mark.asyncio()
async def test_resume_index_follows_chain_length_not_blocks_received() -> None:
    cache = LedgerActivity()
    # Block list shorter than the chain: resuming off len(blocks) re-reads forever.
    transport = ledger_transport(
        [
            {"blocks": [block(1, "esp32-01", NOW)], "chain_length": 50},
            {"blocks": [], "chain_length": 50},
        ]
    )
    await refresh_with(cache, transport)
    await refresh_with(cache, transport)

    assert transport.calls == ["0", "50"]


@pytest.mark.asyncio()
async def test_last_seen_never_moves_backwards() -> None:
    cache = LedgerActivity()
    transport = ledger_transport(
        [
            {
                "blocks": [
                    block(1, "esp32-01", NOW),
                    block(2, "esp32-01", NOW - timedelta(hours=1)),  # out of order
                ],
                "chain_length": 3,
            }
        ]
    )
    await refresh_with(cache, transport)

    assert cache.last_seen("esp32-01") == NOW


@pytest.mark.asyncio()
async def test_an_unreachable_ledger_keeps_the_cache_and_reports_it(window_300) -> None:
    cache = LedgerActivity()
    await refresh_with(
        cache,
        ledger_transport(
            [{"blocks": [block(1, "esp32-01", NOW - timedelta(seconds=30))], "chain_length": 2}]
        ),
    )
    assert cache.reachable is True

    def fail(request: httpx.Request) -> httpx.Response:
        raise httpx.ConnectError("storage-core unreachable")

    await refresh_with(cache, httpx.MockTransport(fail))

    # Clearing would report a healthy fleet as inactive after one blip.
    assert cache.reachable is False
    assert cache.last_seen("esp32-01") == NOW - timedelta(seconds=30)
    assert cache.status("esp32-01", now=NOW) == "active"


@pytest.mark.asyncio()
async def test_reachable_recovers_after_a_failure() -> None:
    cache = LedgerActivity()

    def fail(request: httpx.Request) -> httpx.Response:
        raise httpx.ConnectError("down")

    await refresh_with(cache, httpx.MockTransport(fail))
    assert cache.reachable is False

    await refresh_with(cache, ledger_transport([{"blocks": [], "chain_length": 1}]))
    assert cache.reachable is True


@pytest.mark.asyncio()
async def test_a_malformed_timestamp_is_skipped_not_fatal() -> None:
    cache = LedgerActivity()
    bad = block(1, "esp32-01", NOW)
    bad["timestamp"] = "not-a-timestamp"
    good = block(2, "esp32-02", NOW)

    await refresh_with(
        cache, ledger_transport([{"blocks": [bad, good], "chain_length": 3}])
    )

    assert cache.last_seen("esp32-01") is None
    assert cache.last_seen("esp32-02") == NOW


@pytest.mark.asyncio()
async def test_orphan_actors_are_tracked_for_later_surfacing() -> None:
    cache = LedgerActivity()
    await refresh_with(
        cache,
        ledger_transport(
            [{"blocks": [block(1, "typo-in-secrets-h", NOW)], "chain_length": 2}]
        ),
    )

    # A typo in a device's secrets.h produces events belonging to no device.
    assert "typo-in-secrets-h" in cache.known_actors


@pytest.mark.asyncio()
async def test_genesis_actor_is_harmless() -> None:
    cache = LedgerActivity()
    genesis = block(0, "system", NOW)
    genesis["event"]["event_type"] = "GENESIS"

    await refresh_with(cache, ledger_transport([{"blocks": [genesis], "chain_length": 1}]))

    # "system" matches no registered device, so genesis needs no special case.
    assert cache.last_seen("system") == NOW

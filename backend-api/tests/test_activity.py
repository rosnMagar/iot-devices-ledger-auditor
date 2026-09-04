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


def chain_transport(blocks: list[dict]) -> httpx.MockTransport:
    # Models storage-core for real, including the 400 on a range starting past
    # the last block. The canned-response transport below cannot produce that,
    # which is how IOT-56 slipped through.
    calls: list[str | None] = []

    def handler(request: httpx.Request) -> httpx.Response:
        raw = request.url.params.get("from")
        calls.append(raw)
        start = int(raw) if raw is not None else 0
        last = len(blocks) - 1
        if start > last:
            return httpx.Response(
                400, json={"error": f"invalid range: from={start} exceeds to={last}"}
            )
        return httpx.Response(
            200, json={"blocks": blocks[start:], "chain_length": len(blocks)}
        )

    transport = httpx.MockTransport(handler)
    transport.calls = calls  # type: ignore[attr-defined]
    return transport


def ledger_transport(pages: list[dict]) -> httpx.MockTransport:
    # One canned /blocks response per call; records each `from` param.
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

    # The second call asks for the tail, not the start. It trails by one block
    # on purpose — see IOT-56.
    assert transport.calls == ["0", "1"]
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

    assert transport.calls == ["0", "49"]


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


@pytest.mark.asyncio()
async def test_repeated_refresh_with_no_new_blocks_stays_reachable() -> None:
    # IOT-56. The cursor used to land one past the last index, so a poll with
    # nothing new asked for a range storage-core rejects — and ledger_reachable
    # went false whenever no event had arrived since the last request.
    cache = LedgerActivity()
    transport = chain_transport([block(0, "system", NOW), block(1, "esp32-01", NOW)])

    for _ in range(4):
        await refresh_with(cache, transport)
        assert cache.reachable is True

    assert cache.last_seen("esp32-01") == NOW
    # Never asks for an index past the end.
    assert all(int(c) <= 1 for c in transport.calls)


@pytest.mark.asyncio()
async def test_new_blocks_are_still_picked_up_after_catching_up() -> None:
    cache = LedgerActivity()
    blocks = [block(0, "system", NOW - timedelta(hours=1))]
    transport = chain_transport(blocks)

    await refresh_with(cache, transport)
    assert cache.last_seen("esp32-01") is None

    blocks.append(block(1, "esp32-01", NOW))
    await refresh_with(cache, transport)

    assert cache.last_seen("esp32-01") == NOW
    assert cache.reachable is True


@pytest.mark.asyncio()
async def test_a_cursor_past_the_end_rewinds_and_rescans() -> None:
    # The ledger was reset or replaced, leaving the cursor beyond the new chain.
    # Without the rewind every future poll repeats the same bad request forever.
    cache = LedgerActivity()
    long_chain = [block(i, "esp32-01", NOW) for i in range(6)]
    await refresh_with(cache, chain_transport(long_chain))

    short_chain = [block(0, "system", NOW)]
    short = chain_transport(short_chain)

    await refresh_with(cache, short)
    assert cache.reachable is False  # the 400 that triggered the rewind

    await refresh_with(cache, short)
    assert cache.reachable is True   # rescanned from the start
    assert short.calls == ["5", "0"]

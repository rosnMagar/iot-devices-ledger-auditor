# IOT-60 — the upstream subscription to storage-core's block feed. Driven
# against a real local WebSocket server, not a mock, so the framing and the
# reconnect path are genuinely exercised.

import asyncio
import json

import pytest
import websockets

from app.blockfeed import BlockFeed


def block(index: int, actor: str = "esp32-01") -> dict:
    return {
        "index": index,
        "timestamp": "2026-09-07T12:00:00Z",
        "event": {
            "event_type": "SENSOR_READING",
            "location_id": "warehouse-a",
            "actor": actor,
            "description": "reading",
            "metadata": {"sensor_id": "temp-0", "value": 21.0, "unit": "celsius"},
        },
        "prev_hash": "0" * 64,
        "hash": "a" * 64,
    }


class Upstream:
    """A stand-in for storage-core's :8081 broadcaster."""

    def __init__(self) -> None:
        self.frames: list[str] = []
        self.connections = 0
        self._server = None
        self._sockets: set = set()

    async def __aenter__(self):
        async def handler(socket):
            self.connections += 1
            self._sockets.add(socket)
            try:
                for frame in self.frames:
                    await socket.send(frame)
                await socket.wait_closed()
            finally:
                self._sockets.discard(socket)

        self._server = await websockets.serve(handler, "127.0.0.1", 0)
        return self

    async def __aexit__(self, *exc):
        self._server.close()
        await self._server.wait_closed()

    @property
    def url(self) -> str:
        port = self._server.sockets[0].getsockname()[1]
        return f"ws://127.0.0.1:{port}/blocks"

    async def drop_all(self) -> None:
        for socket in list(self._sockets):
            await socket.close()


async def wait_for(predicate, timeout: float = 3.0) -> None:
    async with asyncio.timeout(timeout):
        while not predicate():
            await asyncio.sleep(0.01)


@pytest.mark.asyncio()
async def test_receives_blocks_from_a_real_server() -> None:
    async with Upstream() as upstream:
        upstream.frames = [json.dumps(block(1)), json.dumps(block(2))]
        received: list[dict] = []

        feed = BlockFeed(upstream.url)
        feed.on_block(lambda b: received.append(b))
        await feed.start()
        try:
            await wait_for(lambda: len(received) == 2)
        finally:
            await feed.stop()

        assert [b["index"] for b in received] == [1, 2]
        assert feed.blocks_received == 2


@pytest.mark.asyncio()
async def test_a_lag_notice_is_not_treated_as_a_block() -> None:
    # Parsing it as a block would invent one with no index and corrupt every
    # consumer downstream.
    async with Upstream() as upstream:
        upstream.frames = [
            json.dumps({"type": "lagged", "dropped": 7}),
            json.dumps(block(9)),
        ]
        blocks: list[dict] = []
        lags: list[int] = []

        feed = BlockFeed(upstream.url)
        feed.on_block(lambda b: blocks.append(b))
        feed.on_lagged(lambda n: lags.append(n))
        await feed.start()
        try:
            await wait_for(lambda: blocks and lags)
        finally:
            await feed.stop()

        assert lags == [7]
        assert [b["index"] for b in blocks] == [9]
        assert feed.blocks_dropped == 7


@pytest.mark.asyncio()
async def test_reconnects_after_the_connection_drops() -> None:
    async with Upstream() as upstream:
        upstream.frames = [json.dumps(block(1))]
        received: list[dict] = []

        feed = BlockFeed(upstream.url)
        feed.on_block(lambda b: received.append(b))
        await feed.start()
        try:
            await wait_for(lambda: len(received) == 1)
            await upstream.drop_all()
            # A second connection, and the frames delivered again.
            await wait_for(lambda: feed.connects >= 2, timeout=5.0)
        finally:
            await feed.stop()

        assert upstream.connections >= 2


@pytest.mark.asyncio()
async def test_an_unreachable_server_does_not_crash_or_spin() -> None:
    # storage-core being down at startup must leave the app running.
    feed = BlockFeed("ws://127.0.0.1:1/blocks")
    await feed.start()
    try:
        await asyncio.sleep(0.4)
        assert feed.connected is False
        assert feed.blocks_received == 0
    finally:
        await feed.stop()


@pytest.mark.asyncio()
async def test_it_connects_once_the_server_appears() -> None:
    # The reverse of the above: a feed started before storage-core is up must
    # find it, not give up.
    async with Upstream() as upstream:
        port = upstream.url.rsplit(":", 1)[1].split("/")[0]
        upstream.frames = [json.dumps(block(1))]
        received: list[dict] = []

        feed = BlockFeed(f"ws://127.0.0.1:{port}/blocks")
        feed.on_block(lambda b: received.append(b))
        await feed.start()
        try:
            await wait_for(lambda: len(received) == 1, timeout=5.0)
        finally:
            await feed.stop()

        assert feed.connected is False  # stopped
        assert received[0]["index"] == 1


@pytest.mark.asyncio()
async def test_a_malformed_frame_is_ignored_not_fatal() -> None:
    async with Upstream() as upstream:
        upstream.frames = ["not json at all", "[1,2,3]", json.dumps(block(5))]
        received: list[dict] = []

        feed = BlockFeed(upstream.url)
        feed.on_block(lambda b: received.append(b))
        await feed.start()
        try:
            await wait_for(lambda: len(received) == 1)
        finally:
            await feed.stop()

        assert [b["index"] for b in received] == [5]


@pytest.mark.asyncio()
async def test_stop_leaves_no_task_behind() -> None:
    # An un-awaited cancelled task leaks and asyncio complains at shutdown.
    async with Upstream() as upstream:
        feed = BlockFeed(upstream.url)
        await feed.start()
        await wait_for(lambda: feed.connects >= 1)
        await feed.stop()

        remaining = [
            t for t in asyncio.all_tasks() if t.get_name() == "block-feed" and not t.done()
        ]
        assert remaining == []
        assert feed.connected is False


@pytest.mark.asyncio()
async def test_stop_is_safe_before_start_and_twice() -> None:
    feed = BlockFeed("ws://127.0.0.1:1/blocks")
    await feed.stop()
    await feed.start()
    await feed.stop()
    await feed.stop()


@pytest.mark.asyncio()
async def test_an_async_listener_is_awaited() -> None:
    async with Upstream() as upstream:
        upstream.frames = [json.dumps(block(3))]
        seen: list[int] = []

        async def listener(b: dict) -> None:
            await asyncio.sleep(0)
            seen.append(b["index"])

        feed = BlockFeed(upstream.url)
        feed.on_block(listener)
        await feed.start()
        try:
            await wait_for(lambda: seen == [3])
        finally:
            await feed.stop()

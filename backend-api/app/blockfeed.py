# Persistent subscription to storage-core's block feed (IOT-60).
#
# storage-core has broadcast new blocks on ws://<host>:8081/blocks since IOT-27/28
# and nothing has ever consumed it. This is its first real client: one upstream
# connection per process, re-established when it drops, dispatching to listeners.
# Fanning out to browsers is IOT-61.

import asyncio
import contextlib
import json
import logging
import os
import random
from collections.abc import Awaitable, Callable

import websockets

logger = logging.getLogger(__name__)

STORAGE_CORE_WS_URL = os.environ.get(
    "STORAGE_CORE_WS_URL", "ws://storage-core:8081/blocks"
)

# Backoff bounds. The ceiling matters more than the floor: a tight reconnect loop
# against a storage-core that is down would spin a core and flood the logs.
INITIAL_BACKOFF = 0.5
MAX_BACKOFF = 30.0

BlockListener = Callable[[dict], Awaitable[None] | None]
LagListener = Callable[[int], Awaitable[None] | None]


async def _call(listener, argument) -> None:
    result = listener(argument)
    if asyncio.iscoroutine(result):
        await result


class BlockFeed:
    def __init__(self, url: str | None = None, connect=None) -> None:
        self._url = url or STORAGE_CORE_WS_URL
        # Injectable so tests can drive a real local server, or a failing connect.
        self._connect = connect or websockets.connect
        self._task: asyncio.Task | None = None
        self._block_listeners: list[BlockListener] = []
        self._lag_listeners: list[LagListener] = []
        self._connected = False
        # Counters, not just a flag: "connected now" hides a feed that is
        # flapping, which looks healthy on any single check.
        self.connects = 0
        self.blocks_received = 0
        self.blocks_dropped = 0

    @property
    def connected(self) -> bool:
        return self._connected

    def on_block(self, listener: BlockListener) -> None:
        self._block_listeners.append(listener)

    def on_lagged(self, listener: LagListener) -> None:
        self._lag_listeners.append(listener)

    async def start(self) -> None:
        if self._task is None or self._task.done():
            self._task = asyncio.create_task(self._run(), name="block-feed")

    async def stop(self) -> None:
        task, self._task = self._task, None
        if task is None:
            return
        task.cancel()
        # Awaited, not just cancelled: leaving it unawaited leaks the task and
        # asyncio complains about it at interpreter shutdown.
        with contextlib.suppress(asyncio.CancelledError):
            await task
        self._connected = False

    async def _run(self) -> None:
        backoff = INITIAL_BACKOFF
        while True:
            try:
                async with self._connect(self._url) as socket:
                    # Reset only after a successful connect: resetting on attempt
                    # would turn a connect/drop loop into a hot loop.
                    backoff = INITIAL_BACKOFF
                    self._connected = True
                    self.connects += 1
                    logger.info("block feed connected to %s", self._url)
                    async for message in socket:
                        await self._dispatch(message)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001 — any failure means retry
                # Includes storage-core being down at startup. The app must come
                # up regardless; a missing live feed is degraded, not fatal.
                logger.warning("block feed disconnected (%s); retrying in %.1fs", exc, backoff)
            finally:
                self._connected = False

            # Jitter so several backends do not reconnect in lockstep after a
            # storage-core restart.
            await asyncio.sleep(backoff * (0.5 + random.random()))
            backoff = min(backoff * 2, MAX_BACKOFF)

    async def _dispatch(self, message: str | bytes) -> None:
        try:
            payload = json.loads(message)
        except (TypeError, ValueError):
            logger.warning("block feed sent unparseable frame; ignoring")
            return
        if not isinstance(payload, dict):
            return

        # A lag notice is not a block. Parsing it as one would invent a block
        # with no index and corrupt every consumer downstream.
        if payload.get("type") == "lagged":
            dropped = payload.get("dropped")
            dropped = dropped if isinstance(dropped, int) else 0
            self.blocks_dropped += dropped
            logger.warning("block feed lagged; storage-core dropped %d blocks", dropped)
            for listener in self._lag_listeners:
                await _call(listener, dropped)
            return

        self.blocks_received += 1
        for listener in self._block_listeners:
            await _call(listener, payload)

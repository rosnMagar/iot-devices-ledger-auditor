# Fan the upstream block feed out to browsers (IOT-61).
#
# One subscription to storage-core (IOT-60) serves every connected dashboard.
# The shape mirrors storage-core's own Subscription: a bounded per-consumer
# queue that drops its oldest entry when full, so one slow browser cannot stall
# the upstream reader and therefore cannot stall every other browser.

import asyncio
import logging
import time

logger = logging.getLogger(__name__)

# Frames buffered per browser before the oldest is dropped. Same figure as
# storage-core's broadcaster, for the same reason: enough to ride out a stutter,
# not enough to hide a consumer that has genuinely stopped keeping up.
QUEUE_CAPACITY = 64

# How long a registry lookup is reused. Enrichment happens per block, and a
# database round trip per block would make the feed's cost scale with traffic.
# Short enough that registering a device shows up promptly.
CACHE_TTL_SECONDS = 30.0


class Subscription:
    def __init__(self, capacity: int = QUEUE_CAPACITY) -> None:
        self._queue: asyncio.Queue = asyncio.Queue(maxsize=capacity)
        self.dropped = 0

    def put(self, frame: dict) -> None:
        # Never awaits: the caller is the single upstream reader, and blocking it
        # on a slow browser would stall the feed for everyone.
        try:
            self._queue.put_nowait(frame)
        except asyncio.QueueFull:
            try:
                self._queue.get_nowait()  # drop the oldest, keep the newest
            except asyncio.QueueEmpty:
                pass
            self.dropped += 1
            try:
                self._queue.put_nowait(frame)
            except asyncio.QueueFull:  # pragma: no cover - drained concurrently
                pass

    async def get(self) -> dict:
        return await self._queue.get()

    def qsize(self) -> int:
        return self._queue.qsize()


class BrowserHub:
    def __init__(self) -> None:
        self._subscriptions: set[Subscription] = set()
        self.delivered = 0

    @property
    def subscriber_count(self) -> int:
        return len(self._subscriptions)

    def subscribe(self) -> Subscription:
        subscription = Subscription()
        self._subscriptions.add(subscription)
        return subscription

    def unsubscribe(self, subscription: Subscription) -> None:
        # discard, not remove: a double close must not raise.
        self._subscriptions.discard(subscription)

    def broadcast(self, frame: dict) -> None:
        # Iterate a copy: a listener unsubscribing mid-broadcast would otherwise
        # mutate the set under us.
        for subscription in list(self._subscriptions):
            subscription.put(frame)
        self.delivered += 1


class RegistryEnricher:
    """A raw block names an actor, not a device. The dashboard needs the type and
    the registered location, which only the registry knows."""

    def __init__(self, session_factory=None, ttl: float = CACHE_TTL_SECONDS) -> None:
        self._session_factory = session_factory
        self._ttl = ttl
        self._cache: dict[str, tuple[float, dict | None]] = {}

    def _lookup(self, device_id: str) -> dict | None:
        if self._session_factory is None:
            from app.db import SessionLocal

            self._session_factory = SessionLocal
        from app.models import Device

        with self._session_factory() as session:
            device = session.get(Device, device_id)
            if device is None:
                return None
            return {
                "device_type": device.device_type,
                "location_id": device.location_id,
                "registered": True,
            }

    def device(self, device_id: str) -> dict | None:
        now = time.monotonic()
        cached = self._cache.get(device_id)
        if cached is not None and now - cached[0] < self._ttl:
            return cached[1]
        try:
            found = self._lookup(device_id)
        except Exception as exc:  # noqa: BLE001 — enrichment must never break the feed
            logger.warning("could not enrich block for %r: %s", device_id, exc)
            return None
        # Misses are cached too, with the same TTL: an unregistered actor posting
        # continuously would otherwise hit the database on every block.
        self._cache[device_id] = (now, found)
        return found

    def enrich(self, block: dict) -> dict:
        actor = block.get("event", {}).get("actor")
        device = self.device(actor) if isinstance(actor, str) and actor else None
        return {
            "type": "block",
            "block": block,
            # null when the ledger reports an actor the registry does not know —
            # the same orphan case IOT-35 and IOT-73 already surface.
            "device": device,
        }

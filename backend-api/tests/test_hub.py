# IOT-61 — fanning the block feed out to browsers. The failure that matters is
# one slow consumer stalling everyone else, so that gets the most attention.


import pytest

from app.hub import BrowserHub, RegistryEnricher, Subscription


def frame(index: int) -> dict:
    return {"type": "block", "block": {"index": index}}


def block(actor: str) -> dict:
    return {
        "index": 1,
        "timestamp": "2026-09-07T12:00:00Z",
        "event": {"event_type": "SENSOR_READING", "actor": actor,
                  "location_id": "reported-loc", "metadata": {}},
    }


class TestFanout:
    def test_one_broadcast_reaches_every_subscriber(self) -> None:
        hub = BrowserHub()
        a, b, c = hub.subscribe(), hub.subscribe(), hub.subscribe()

        hub.broadcast(frame(1))

        assert [s.qsize() for s in (a, b, c)] == [1, 1, 1]
        assert hub.subscriber_count == 3

    @pytest.mark.asyncio()
    async def test_subscribers_receive_in_order(self) -> None:
        hub = BrowserHub()
        sub = hub.subscribe()
        for i in range(3):
            hub.broadcast(frame(i))

        received = [(await sub.get())["block"]["index"] for _ in range(3)]
        assert received == [0, 1, 2]

    def test_unsubscribing_stops_delivery(self) -> None:
        hub = BrowserHub()
        stays, goes = hub.subscribe(), hub.subscribe()
        hub.unsubscribe(goes)

        hub.broadcast(frame(1))

        assert stays.qsize() == 1
        assert goes.qsize() == 0

    def test_unsubscribing_twice_is_harmless(self) -> None:
        # A browser closing mid-broadcast must not raise on the second cleanup.
        hub = BrowserHub()
        sub = hub.subscribe()
        hub.unsubscribe(sub)
        hub.unsubscribe(sub)
        assert hub.subscriber_count == 0

    def test_broadcasting_with_no_subscribers_is_fine(self) -> None:
        BrowserHub().broadcast(frame(1))

    def test_a_subscriber_added_mid_stream_gets_only_new_frames(self) -> None:
        hub = BrowserHub()
        early = hub.subscribe()
        hub.broadcast(frame(1))
        late = hub.subscribe()
        hub.broadcast(frame(2))

        assert early.qsize() == 2
        assert late.qsize() == 1


class TestSlowConsumer:
    def test_a_full_queue_drops_the_oldest_rather_than_blocking(self) -> None:
        # The whole point: the upstream reader must never wait on a browser.
        sub = Subscription(capacity=3)
        for i in range(5):
            sub.put(frame(i))

        assert sub.qsize() == 3
        assert sub.dropped == 2

    @pytest.mark.asyncio()
    async def test_the_newest_frames_survive(self) -> None:
        # A live dashboard wants the latest state, not the oldest backlog.
        sub = Subscription(capacity=3)
        for i in range(5):
            sub.put(frame(i))

        kept = [(await sub.get())["block"]["index"] for _ in range(3)]
        assert kept == [2, 3, 4]

    def test_one_slow_consumer_does_not_affect_the_others(self) -> None:
        hub = BrowserHub()
        fast, slow = hub.subscribe(), hub.subscribe()
        # Fill both well past capacity, then let only the fast one drain.
        for i in range(200):
            hub.broadcast(frame(i))
        while fast.qsize():
            fast._queue.get_nowait()
        hub.broadcast(frame(999))

        assert fast.qsize() == 1          # keeping up
        assert slow.dropped > 0           # falling behind, and it is its own problem
        assert hub.subscriber_count == 2  # not disconnected for being slow

    def test_broadcast_never_blocks_even_when_every_queue_is_full(self) -> None:
        hub = BrowserHub()
        for _ in range(5):
            hub.subscribe()
        for i in range(500):
            hub.broadcast(frame(i))  # would hang if put() ever awaited
        assert hub.delivered == 500


class TestEnrichment:
    def enricher(self, devices: dict, calls: list) -> RegistryEnricher:
        enricher = RegistryEnricher(ttl=100.0)
        def lookup(device_id: str):
            calls.append(device_id)
            return devices.get(device_id)
        enricher._lookup = lookup
        return enricher

    def test_a_block_is_enriched_from_the_registry(self) -> None:
        # A raw block names an actor; the dashboard needs the type and location.
        calls: list[str] = []
        enricher = self.enricher(
            {"esp32-01": {"device_type": "ESP32", "location_id": "cold-store",
                          "registered": True}}, calls)

        result = enricher.enrich(block("esp32-01"))

        assert result["type"] == "block"
        assert result["device"]["device_type"] == "ESP32"
        # The registry is authoritative for placement, not the reported field.
        assert result["device"]["location_id"] == "cold-store"
        assert result["block"]["event"]["location_id"] == "reported-loc"

    def test_an_unregistered_actor_enriches_to_null_not_an_error(self) -> None:
        calls: list[str] = []
        result = self.enricher({}, calls).enrich(block("typo-in-config"))

        assert result["device"] is None
        assert result["block"]["event"]["actor"] == "typo-in-config"

    def test_lookups_are_cached(self) -> None:
        # A database round trip per block would make cost scale with traffic.
        calls: list[str] = []
        enricher = self.enricher(
            {"esp32-01": {"device_type": "ESP32", "location_id": None,
                          "registered": True}}, calls)
        for _ in range(50):
            enricher.enrich(block("esp32-01"))

        assert calls == ["esp32-01"]

    def test_misses_are_cached_too(self) -> None:
        # Otherwise an unregistered actor posting continuously hammers the DB.
        calls: list[str] = []
        enricher = self.enricher({}, calls)
        for _ in range(50):
            enricher.enrich(block("ghost"))

        assert calls == ["ghost"]

    def test_the_cache_expires(self) -> None:
        calls: list[str] = []
        enricher = self.enricher({}, calls)
        enricher._ttl = 0.0
        enricher.enrich(block("later"))
        enricher.enrich(block("later"))

        assert len(calls) == 2

    def test_a_registry_failure_does_not_break_the_feed(self) -> None:
        # A dead database must degrade the feed, not stop it.
        enricher = RegistryEnricher(ttl=100.0)
        def boom(device_id: str):
            raise RuntimeError("database gone")
        enricher._lookup = boom

        result = enricher.enrich(block("esp32-01"))

        assert result["device"] is None
        assert result["block"]["index"] == 1

    def test_a_block_with_no_actor_is_still_forwarded(self) -> None:
        result = RegistryEnricher(ttl=100.0).enrich({"index": 0, "event": {}})
        assert result["device"] is None
        assert result["block"]["index"] == 0

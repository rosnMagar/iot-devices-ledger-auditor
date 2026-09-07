# Device activity derived from the ledger. last_seen is not stored — storage-core
# owns the chain, and a cached column would go stale silently.

import logging
import os
from collections import deque
from datetime import datetime, timedelta, timezone

import httpx

logger = logging.getLogger(__name__)

STORAGE_CORE_URL = os.environ.get("STORAGE_CORE_URL", "http://localhost:8080")
ACTIVE_WINDOW_SECONDS = int(os.environ.get("ACTIVE_WINDOW_SECONDS", "300"))
# Readings kept in memory per sensor. Bounded on purpose: the ledger grows
# without limit and this is a cache, not storage. Older readings are still in
# the chain — they are simply not served by /readings.
HISTORY_LIMIT = int(os.environ.get("READING_HISTORY_LIMIT", "500"))


def _parse_timestamp(raw: str) -> datetime | None:
    try:
        parsed = datetime.fromisoformat(raw)
    except (TypeError, ValueError):
        logger.warning("skipping block with unparseable timestamp: %r", raw)
        return None
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


class LedgerActivity:
    # Incremental: the ledger is append-only, so consumed blocks can never
    # change and only the tail needs fetching.
    def __init__(self, base_url: str | None = None) -> None:
        self._base_url = (base_url or STORAGE_CORE_URL).rstrip("/")
        self._last_seen: dict[str, datetime] = {}
        # (device_id, sensor_id) -> the most recent reading seen for it.
        self._latest: dict[tuple[str, str], dict] = {}
        # (device_id, sensor_id) -> bounded history, oldest first.
        self._history: dict[tuple[str, str], deque] = {}
        self._next_index = 0
        self._reachable = True

    @property
    def reachable(self) -> bool:
        return self._reachable

    @property
    def known_actors(self) -> set[str]:
        # Includes actors with no devices row — a typo in a device's secrets.h.
        return set(self._last_seen)

    def last_seen(self, device_id: str) -> datetime | None:
        return self._last_seen.get(device_id)

    def status(self, device_id: str, now: datetime | None = None) -> str:
        # Two states only. Never-reported is inactive with last_seen None.
        seen = self._last_seen.get(device_id)
        if seen is None:
            return "inactive"
        reference = now or datetime.now(timezone.utc)
        return (
            "active"
            if reference - seen <= timedelta(seconds=ACTIVE_WINDOW_SECONDS)
            else "inactive"
        )

    async def refresh(self, client: httpx.AsyncClient | None = None) -> None:
        owns_client = client is None
        client = client or httpx.AsyncClient()
        try:
            # from = _next_index - 1, not _next_index. storage-core 400s on a
            # range starting past the last block, and _next_index is exactly one
            # past it once we have caught up — so asking for it would 400 on
            # every poll where nothing new arrived (IOT-56). Re-reading the last
            # block is harmless: _consume keeps the max timestamp.
            response = await client.get(
                f"{self._base_url}/blocks",
                params={"from": max(0, self._next_index - 1)},
                timeout=5.0,
            )
            response.raise_for_status()
            payload = response.json()
        except httpx.HTTPStatusError as exc:
            self._reachable = False
            if exc.response.status_code == 400:
                # Our cursor is past the end of the chain — the ledger was reset
                # or replaced. Rewind so the next refresh rescans from scratch,
                # otherwise every future poll repeats this same bad request.
                logger.warning("ledger cursor past end of chain; rescanning next refresh")
                self._next_index = 0
            else:
                logger.warning("could not refresh ledger activity: %s", exc)
            return
        except (httpx.HTTPError, ValueError) as exc:
            # Keep the cache: clearing it would report the whole fleet inactive
            # after one blip.
            self._reachable = False
            logger.warning("could not refresh ledger activity: %s", exc)
            return
        finally:
            if owns_client:
                await client.aclose()

        self._reachable = True
        self._consume(payload)

    def latest_reading(self, device_id: str, sensor_id: str) -> dict | None:
        return self._latest.get((device_id, sensor_id))

    def sensors_seen(self, device_id: str) -> set[str]:
        # Sensor ids the ledger has reported for this device, registered or not.
        return {sid for (did, sid) in self._latest if did == device_id}

    def readings(
        self,
        device_id: str,
        sensor_id: str | None = None,
        since: datetime | None = None,
        limit: int = HISTORY_LIMIT,
    ) -> list[dict]:
        """Recent readings, oldest first. Across all of a device's sensors when
        sensor_id is None, so one request can back a whole panel."""
        keys = (
            [(device_id, sensor_id)]
            if sensor_id is not None
            else [k for k in self._history if k[0] == device_id]
        )
        collected: list[dict] = []
        for key in keys:
            for reading in self._history.get(key, ()):
                if since is not None and reading["at"] <= since:
                    continue
                collected.append(reading)

        # Sorted across sensors, then trimmed from the *newest* end — a chart
        # wants the recent tail, not the oldest points that happen to fit.
        collected.sort(key=lambda r: (r["at"], r["sensor_id"], r["seq"] or 0))
        return collected[-limit:] if limit > 0 else []

    def _consume(self, payload: dict) -> None:
        for block in payload.get("blocks", []):
            event = block.get("event", {})
            actor = event.get("actor")
            if not actor:
                continue
            timestamp = _parse_timestamp(block.get("timestamp", ""))
            if timestamp is None:
                continue
            # max, not assign: out-of-order delivery must not rewind last_seen.
            current = self._last_seen.get(actor)
            if current is None or timestamp > current:
                self._last_seen[actor] = timestamp
            self._consume_reading(actor, event, timestamp)

        # Resume from chain_length, not len(blocks) — a truncated response would
        # otherwise re-read the tail forever.
        chain_length = payload.get("chain_length")
        if isinstance(chain_length, int) and chain_length >= 0:
            self._next_index = chain_length

    def _consume_reading(self, actor: str, event: dict, timestamp: datetime) -> None:
        # Only measurements. CAMERA_EVENTs carry no value (ADR 0011), and blocks
        # written before ADR 0010 conform to nothing — both are skipped rather
        # than being coerced into a reading.
        if event.get("event_type") != "SENSOR_READING":
            return
        metadata = event.get("metadata")
        if not isinstance(metadata, dict):
            return
        sensor_id = metadata.get("sensor_id")
        if not isinstance(sensor_id, str) or not sensor_id:
            return

        key = (actor, sensor_id)
        previous = self._latest.get(key)
        if previous is not None and previous["at"] > timestamp:
            return  # out-of-order delivery must not rewind the latest reading
        reading = {
            "sensor_id": sensor_id,
            "sensor_type": metadata.get("sensor_type"),
            # None is a real, meaningful value here: the sensor failed (ADR 0010).
            "value": metadata.get("value"),
            "unit": metadata.get("unit"),
            "seq": metadata.get("seq"),
            "at": timestamp,
        }
        self._latest[key] = reading
        if key not in self._history:
            self._history[key] = deque(maxlen=HISTORY_LIMIT)
        self._history[key].append(reading)

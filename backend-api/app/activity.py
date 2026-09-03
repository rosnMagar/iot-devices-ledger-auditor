# Device activity derived from the ledger. last_seen is not stored — storage-core
# owns the chain, and a cached column would go stale silently.

import logging
import os
from datetime import datetime, timedelta, timezone

import httpx

logger = logging.getLogger(__name__)

STORAGE_CORE_URL = os.environ.get("STORAGE_CORE_URL", "http://localhost:8080")
ACTIVE_WINDOW_SECONDS = int(os.environ.get("ACTIVE_WINDOW_SECONDS", "300"))


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
            response = await client.get(
                f"{self._base_url}/blocks",
                params={"from": self._next_index},
                timeout=5.0,
            )
            response.raise_for_status()
            payload = response.json()
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

    def _consume(self, payload: dict) -> None:
        for block in payload.get("blocks", []):
            actor = block.get("event", {}).get("actor")
            if not actor:
                continue
            timestamp = _parse_timestamp(block.get("timestamp", ""))
            if timestamp is None:
                continue
            # max, not assign: out-of-order delivery must not rewind last_seen.
            current = self._last_seen.get(actor)
            if current is None or timestamp > current:
                self._last_seen[actor] = timestamp

        # Resume from chain_length, not len(blocks) — a truncated response would
        # otherwise re-read the tail forever.
        chain_length = payload.get("chain_length")
        if isinstance(chain_length, int) and chain_length >= 0:
            self._next_index = chain_length

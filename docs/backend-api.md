# backend-api — Status: In Progress (Phase 2)

## Overview

Phase 0 was a stub. Phase 2 is underway: the relational models are real (IOT-34)
and `GET /devices` serves the device registry joined to activity derived from the
ledger (IOT-35). `GET /users` is still hardcoded and moves across with the auth
work that populates it.

## Endpoints

| Endpoint | Status | Notes |
|---|---|---|
| `GET /health` | real | liveness only |
| `GET /blocks` | real | proxies storage-core via `httpx`; 502 if unreachable |
| `GET /devices` | real (IOT-35) | registry + derived activity — see below |
| `GET /users` | **stub** | hardcoded; the table exists but starts empty |

### `GET /devices`

```json
{
  "devices": [
    {
      "device_id": "esp32-01",
      "location_id": "loc-1",
      "device_type": "DHT22",
      "registered_at": "2026-09-03T17:30:28+00:00",
      "last_seen": "2026-09-03T17:30:35+00:00",
      "status": "active"
    }
  ],
  "active_window_seconds": 300,
  "ledger_reachable": true
}
```

**`status` is derived, never stored.** storage-core owns the hash chain and is
the source of truth for what happened; a `last_seen` column here would need
something to keep it true and would go stale silently when that failed. So it is
computed from the most recent block whose `actor` is the device.

- `status` is `active` if `last_seen` falls within `ACTIVE_WINDOW_SECONDS`
  (default 300), otherwise `inactive`. Only two values.
- `last_seen: null` means the device has **never** reported — distinct from
  having gone quiet, which is also `inactive` but carries a timestamp.
- `ledger_reachable: false` means storage-core could not be reached, so the
  activity shown is as of the last successful refresh. The registry is still
  correct. This is reported rather than hidden: a dashboard showing stale
  "active" badges as though they were current is the failure mode to avoid.

Filtering and sorting arrive in IOT-36.

**Reading the ledger.** The activity cache is incremental: it remembers the next
block index it has not consumed and asks storage-core for `/blocks?from=<that>`,
which works because the ledger is append-only — a block, once written, never
changes, so anything already consumed can never be wrong. Steady state is a
request that returns nothing; a restart costs one full scan.

Deliberately *not* subscribed to the WebSocket feed. That would be
lower-latency and is the natural next move, but it needs a background task,
reconnect handling, and a story for what the cache holds while disconnected.

## Stack / Decisions

- Python, FastAPI
- SQLite via SQLAlchemy for `users`, `locations`, `settings`, `devices` (see [`db-schema.md`](db-schema.md)); documented Postgres migration path
- Proxies storage-core's `/events` and `/blocks`
- WebSocket relay from storage-core's `ws://<host>:8081/blocks` to the frontend
- CORS enabled for the frontend origin (`CORS_ORIGINS` env var)
- `ACTIVE_WINDOW_SECONDS` (default 300) sets the activity window. A window
  shorter than the firmware's read interval marks healthy devices inactive
  between readings.

## Open Questions

- Auth model for users (Phase 2 design)
- Postgres migration trigger/timeline
- What to do with **orphan actors** — ledger events whose `actor` matches no
  registered device, which is what a typo in a device's `secrets.h` produces.
  They are tracked by the activity cache but appear nowhere, since `/devices`
  lists the registry. Surfacing them would turn a silent misconfiguration into a
  visible one.

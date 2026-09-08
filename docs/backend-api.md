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

#### Query parameters (IOT-36)

| Param | Values | Default |
|---|---|---|
| `status` | `active`, `inactive`, `all` | `all` |
| `location_id` | any location id | unset (no filter) |
| `device_type` | any device type | unset (no filter) |
| `sort` | `last_seen`, `name`, `location` | `last_seen` |
| `order` | `asc`, `desc` | `desc` |

An invalid `status`, `sort` or `order` is a **400** naming the allowed values.
An unknown parameter is ignored, so a typo'd filter fails open to the full list
rather than erroring.

`sort=name` orders by `device_id` — devices have no separate name column.
`device_id` is also the tiebreaker for every sort, so results are deterministic.

Devices that have never reported (`last_seen: null`) sort as the oldest possible
time: last under the default `last_seen`/`desc`, first under `asc`.

`location_id` and `device_type` filter in SQL (both columns are indexed).
`status` and every sort run in Python, because they depend on values derived
from the ledger that SQL cannot see. Fine for a fleet; if the registry ever
grows past what is comfortable to hold in memory, `last_seen` would need
materialising — which is the thing IOT-34 deliberately avoided, so it should be
a considered change rather than a quiet one.

The response echoes the applied `filters`, `sort`, `order` and a `count`.

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

## `GET /devices/{device_id}/sensors`

A device's registered sensors, each with its latest reading (IOT-73). This is
what the dashboard panel calls on first paint — the full series is `/readings`.

```json
{
  "device_id": "esp32-03",
  "sensors": [
    {"sensor_id": "acce-0", "sensor_type": "accelerometer", "unit": "m_s2",
     "registered": true,
     "latest": {"value": [-0.88, -1.01, 8.6], "unit": "m_s2", "seq": 3,
                "at": "2026-09-07T19:58:31+00:00"}},
    {"sensor_id": "temp-0", "sensor_type": "temperature", "unit": "celsius",
     "registered": true,
     "latest": {"value": 20.66, "unit": "celsius", "seq": 1, "at": "..."}}
  ],
  "count": 3,
  "unregistered_count": 0,
  "ledger_reachable": true
}
```

- `latest` is `null` when a registered sensor has never reported. A `latest`
  whose `value` is `null` is different: the sensor reported and the read failed
  (ADR 0010).
- `unit` on the sensor is what it is *expected* to report; the one inside
  `latest` is authoritative, so a swapped probe is visible in the data.
- `registered: false` entries are sensors the **ledger** has reported that no
  registered sensor matches — a typo in a firmware config. They are surfaced
  rather than hidden, because that is how the typo gets found.
- Unknown device → `404`.

`GET /devices` also carries a per-device `sensor_count` and sorted
`sensor_types` summary, so the table can show what a board carries without a
request per row.

## `GET /readings`

Recent readings for a device, oldest first (IOT-62) — the history a chart draws
before anything arrives over the live feed.

```
GET /readings?device_id=esp32-03&sensor_id=acce-0&since=<iso>&limit=500
```

```json
{
  "device_id": "esp32-03", "sensor_id": "acce-0",
  "series": [
    {"sensor_id": "acce-0", "sensor_type": "accelerometer",
     "value": [-0.7, 0.41, 9.96], "unit": "m_s2", "seq": 1,
     "at": "2026-09-07T20:07:26+00:00"}
  ],
  "count": 6, "limit": 500, "history_limit": 500,
  "truncated": false, "ledger_reachable": true
}
```

- **Omit `sensor_id`** to get every sensor on the device, interleaved by time —
  one request backs a whole panel.
- `value` arrives unchanged: a number, a `[x, y, z]` vector, or `null` when the
  sensor reported and the read failed (ADR 0010). A chart breaks the line on
  `null` rather than plotting zero.
- `CAMERA_EVENT` blocks are not readings and never appear here (ADR 0011).
  Blocks with malformed or missing metadata are skipped, not fatal.
- `truncated` says the series hit `limit`. Trimming takes the **newest** end,
  because a chart wants the recent tail.
- Unknown device → `404`. A `limit` outside `1..history_limit`, or an
  unparseable `since`, → `400`.

### The ceiling

Readings are served from a **bounded in-memory cache**, not from storage:
`READING_HISTORY_LIMIT` (default 500) per sensor. Older readings are still in
the chain — they are simply not served here. The cache is rebuilt from the full
chain on process start, since the ledger cursor begins at 0.

This is the MVP tradeoff. Serving arbitrary history would mean re-scanning a
chain that grows without limit on every request; a bounded cache keeps both
memory and latency flat. A real time-series store is the answer if deeper
history is ever needed.

## Live block feed

backend-api holds one persistent WebSocket subscription to storage-core's
`ws://<host>:8081/blocks` (IOT-60), configured by `STORAGE_CORE_WS_URL`. This is
the first consumer of the broadcaster built in IOT-27/28.

- **One upstream connection per process.** Fanning out to browsers is IOT-61.
- **Reconnects with exponential backoff and jitter** (0.5s to 30s). The ceiling
  matters more than the floor: a tight loop against a dead storage-core would
  spin a core and flood the logs. Jitter stops several backends reconnecting in
  lockstep after a restart.
- **storage-core being down at startup is not fatal.** The app starts and the
  feed retries; a missing live feed is degraded, not broken.
- **A `{"type":"lagged","dropped":N}` notice is not a block.** Parsing it as one
  would invent a block with no index and corrupt every consumer downstream.

`GET /feed/status` reports `connected`, `connects`, `blocks_received` and
`blocks_dropped`. The counters matter as much as the flag: "connected right now"
hides a feed that is flapping, which looks healthy on any single check.

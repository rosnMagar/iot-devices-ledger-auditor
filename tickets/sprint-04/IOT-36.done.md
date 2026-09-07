# IOT-36: backend-api — `GET /devices` filters + `sort`/`order`

**Sprint:** sprint-04
**Story points:** 3
**Status:** Review
**Depends on:** IOT-35

## Story
As a dashboard, I want to query devices with filtering and sorting so that operators can find active devices fast.

## Acceptance criteria
- [x] `GET /devices` supports filters: `status` (active/inactive/all), `location_id`, `device_type`
- [x] Supports `sort` (`last_seen`/`name`/`location`) + `order` (`asc`/`desc`)
- [x] Defaults: `status=all`, `sort=last_seen`, `order=desc`
- [x] Invalid `sort`/`order`/`status` → 400 with a clear message

## Implementation notes
- Apply filters at the query level where possible; `status` may need post-filtering on derived `last_seen`.
- Document params in `docs/backend-api.md`.

## Where each operation runs
`location_id` and `device_type` filter in SQL — both columns are indexed
(IOT-34 indexed them for exactly this). `status` and every sort run in Python,
because they depend on `last_seen`, which is derived from the ledger and not a
column SQL can see.

Sorting entirely in Python rather than splitting it between SQL and memory keeps
one ordering path instead of two that must agree. Fine at fleet scale; if the
registry outgrows memory, `last_seen` would have to be materialised — the thing
IOT-34 deliberately avoided, so it should be a considered change rather than a
quiet one. Noted in `docs/backend-api.md`.

## Two decisions the ticket left open
**`sort=name` maps to `device_id`.** `Device` has no `name` column — it is
`device_id`, `location_id`, `device_type`, `registered_at`. Adding one to satisfy
the sort key would have expanded IOT-34's deliberate schema. Documented.

**Never-reported devices sort as the oldest possible time**, so they land last
under the default `last_seen`/`desc` and first under `asc`. The alternative —
always forcing nulls last — reads better in one direction and is surprising in
the other. `device_id` is the tiebreaker for every sort, so ties are
deterministic.

## Testing
`ruff check` clean, **48 passed** (14 new in `tests/test_devices_query.py`).

Covers: defaults, each filter alone, filters combined, both sort directions for
all three keys, never-seen placement under `asc`, determinism on ties, 400s for
each invalid parameter with the allowed values named, unknown params ignored,
and the echoed `filters`/`sort`/`order`/`count`.

### End-to-end against real services
A four-device fleet against a live storage-core, two of them reporting:
- default → `charlie, alpha` active then `delta, bravo` inactive
- `status=active` → 2; `status=inactive` → 2
- `location_id=warehouse` → 2; `device_type=DHT22` → 3
- all three filters combined → `alpha` only
- `sort=name&order=asc` → alphabetical; `sort=last_seen&order=asc` → never-seen first
- `status=online` → 400 `status must be one of active, inactive, all (got 'online')`
- same for `sort` and `order`

## Note
An unknown query parameter is ignored rather than rejected — FastAPI's default.
A typo'd filter therefore fails open to the full list instead of erroring. There
is a test pinning that so it is a known behaviour rather than an accident;
rejecting unknown params would be a defensible change but a different one.

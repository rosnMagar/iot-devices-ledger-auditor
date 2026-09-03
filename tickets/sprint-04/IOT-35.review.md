# IOT-35: backend-api — define "active device" + derive `last_seen`

**Sprint:** sprint-04
**Story points:** 3
**Status:** Review
**Depends on:** IOT-34, IOT-22

## Story
As an operator, I want to know which devices are currently active so that I can monitor the fleet.

## Acceptance criteria
- [x] A device's `last_seen` is derived from its most recent event in storage-core (`GET /blocks` filtered by `actor`/device)
- [x] "Active" = `last_seen` within a configurable window (e.g. `ACTIVE_WINDOW_SECONDS`, default 300)
- [x] Each device record exposes `status` (active/inactive) + `last_seen`
- [x] Window is configurable via env

## Implementation notes
- Matching: events carry `actor`/`location_id`; map to `devices` rows.
- Consider caching `last_seen` to avoid scanning all blocks per request.

## What was built
- **`app/activity.py`** — `LedgerActivity`, an incremental cache of the most
  recent ledger event per actor, plus `status()` against a configurable window.
- **`GET /devices`** in `app/main.py` — the registry joined to derived activity.
- **`ACTIVE_WINDOW_SECONDS`** (default 300) wired through `.env.example` and
  `docker-compose.yml`.
- **`docs/backend-api.md`** rewritten from "Not Started" to document the endpoint,
  the response shape and the derivation.

## Decision: incremental pull, not a full scan and not the WebSocket
Scanning every block per request is correct and gets slower forever. The ledger
is **append-only**, so a block once written never changes — anything already
consumed can never be wrong, and only the tail needs fetching. The cache holds
the next unconsumed index and asks for `/blocks?from=<that>`. Steady state is a
request returning nothing; a restart costs one full scan.

`chain_length` from the response drives the resume point, **not** the number of
blocks received — deriving it from the blocks would silently re-read the tail
forever if a response were ever truncated. There is a test for that specifically.

Rejected for now: subscribing to the `:8081` WebSocket feed. Lower latency and
the natural next move, but it needs a background task, reconnect handling and a
story for what the cache holds while disconnected. The pull model needs none of
that and is accurate to the second the request is made.

## Decision: two statuses, not three
`active` / `inactive` only. A device that has never reported is `inactive` with
`last_seen: null` — the null carries "never showed up" versus "went quiet"
without giving IOT-36's filters a third case to handle.

## Decision: degrade visibly, not silently
If storage-core is unreachable the registry is still correct, so failing the
whole request would be worse than serving it. But the cache is **kept, not
cleared** — clearing it would report a healthy fleet as entirely inactive after
one network blip — and the response carries `ledger_reachable: false`. A
dashboard showing confidently stale "active" badges is exactly the quiet failure
IOT-49/51/52 were about.

## Scope note: `GET /devices` was created here
The ticket says "each device record exposes `status` + `last_seen`" without
saying which ticket creates the endpoint; IOT-36 is titled "filters +
sort/order", which reads as additions to something that exists. So IOT-35 builds
the plain endpoint and IOT-36 is purely additive. Without it nothing here would
be observable.

## Testing
`tests/test_activity.py` — 11 cases driving `LedgerActivity` through an
`httpx.MockTransport`: last_seen from the most recent block, the window boundary
(inclusive), never-reported devices, **resume-from-index rather than rescan**
(asserted on the actual `from` params: `["0", "2"]`), resume following
`chain_length` not blocks received, last_seen never moving backwards, unreachable
ledger keeping the cache and flagging it, recovery afterwards, a malformed
timestamp being skipped rather than fatal, orphan actors tracked, and genesis
being harmless.

`tests/test_devices_endpoint.py` — 5 cases over the endpoint with the DB and
ledger both stubbed: registry listed with derived status, the window reported,
activity refreshed per request, an unreachable ledger still serving the registry,
and the empty case.

`ruff check` clean, **34 passed**.

### End-to-end against the real services
storage-core on 18080, backend-api in a container against it:
- two devices registered, `/devices` → both `inactive`, `last_seen: null`,
  `ledger_reachable: true`
- `POST /events` as `esp32-01` → 201, then `/devices` → `esp32-01` **active**
  with a real timestamp, `esp32-quiet` still inactive
- storage-core killed → registry still served, cached `last_seen` retained,
  `ledger_reachable: false`
- same event under `ACTIVE_WINDOW_SECONDS=5` → `inactive`, proving the window is
  actually configurable rather than nominally so

## Fixed while here
The endpoint tests originally used `with TestClient(app)`, whose lifespan calls
`init_db()` against the real `DATABASE_URL` and left a stray `backend-api/app.db`
in the repo. The session dependency is overridden anyway, so the lifespan created
tables nothing read. Dropped the context manager and added `backend-api/*.db` to
`.gitignore`.

## Open question handed to a later ticket
**Orphan actors** — ledger events whose `actor` matches no registered device,
which is what a typo in a device's `secrets.h` produces. `LedgerActivity`
tracks them (`known_actors`) but nothing surfaces them, since `/devices` lists
the registry. Surfacing them would turn a silent misconfiguration into a visible
one. Recorded in `docs/backend-api.md` under Open Questions.

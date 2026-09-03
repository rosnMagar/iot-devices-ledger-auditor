# IOT-34: backend-api — SQLAlchemy models (users/locations/settings/devices)

**Sprint:** sprint-04
**Story points:** 5
**Status:** Review
**Depends on:** —

## Story
As a developer, I want persistent relational models so that users, locations, settings, and devices can be managed.

## Acceptance criteria
- [x] SQLAlchemy configured with SQLite (`DATABASE_URL`), session/engine wiring
- [x] Models: `users`, `locations`, `settings`, `devices` (`device_id`, `location_id` FK, `device_type`, `registered_at`)
- [x] Tables auto-created on startup (or via init script)
- [x] `docs/db-schema.md` updated to match the implemented schema

## Implementation notes
- See `docs/db-schema.md` for the tentative sketch; finalize here.
- Document a Postgres migration path; keep models DB-agnostic.

## What was built
- **`app/db.py`** — engine, `SessionLocal`, `Base`, `init_db()`, and a
  `get_session()` FastAPI dependency.
- **`app/models.py`** — `User`, `Location`, `Setting`, `Device`.
- **`app/main.py`** — a `lifespan` handler calling `init_db()` at boot.
- **`sqlalchemy>=2.0`** added to `requirements.txt` (the ticket mandates
  SQLAlchemy; nothing was installed on the host — see Verification).

## Decision: natural string keys, not integer surrogates
`locations.id` and `devices.device_id` are the exact strings the firmware puts
in a block's `location_id` and `actor`, so correlating a registry row to ledger
events is a direct key match with no translation table.

Rejected: integer surrogate PKs plus a unique slug — more conventional, and it
would let a location be renamed without re-flashing firmware. It loses because
the ledger is **immutable**: historical blocks can never be rewritten to point at
a new key, so the mapping layer would have to exist forever and buy nothing.

Cost, stated plainly: renaming a location means re-flashing its devices, and a
typo in `secrets.h` produces events matching no registered row. **IOT-35 has to
decide what to do with those orphans.**

## Decision: no `last_seen` or `active` column
Activity is derived from the ledger (IOT-35), not stored. A cached flag needs
something to keep it true and goes stale the moment that fails. There is a test
asserting `devices` has exactly four columns, so adding one later is a
deliberate act rather than a drift.

## Two portability bugs found and fixed
**SQLite disables foreign keys by default.** Without a `PRAGMA foreign_keys=ON`
listener, a device could reference a nonexistent location and nothing would
complain — constraints real in production, decorative in development.

**SQLite has no timezone type.** Found by a failing test: a column declared
`DateTime(timezone=True)` stores an aware value and returns a *naive* one, while
Postgres `TIMESTAMPTZ` returns it aware. That is the "works in dev, wrong in
prod" shape IOT-49/51/52 were all about. Fixed with a `UtcDateTime` type
decorator normalising both directions; effectively a pass-through on Postgres.

## Verification
**Nothing was installed on the host.** `pip install` is on the blocked list, and
no Python environment existed locally anyway — everything ran inside throwaway
containers.

- `ruff check .` clean (CI's lint step); `pytest` **16 passed**
- 16 cases cover: all four tables created, device↔location relationship, natural
  string keys, optional placement, FK rejection of an orphan device, uniqueness
  on `device_id`/`username`/`settings.key`, user defaults, tz-aware timestamps,
  `updated_at` moving on change, the four-column assertion, and two regression
  tests for the `UtcDateTime` round-trip
- **Real image built and booted**: `/health` 200, all four tables present after
  startup, FK enforcement proven end to end by rejecting an orphan insert
  through the app's own engine, `init_db()` idempotent, indexes present on
  `device_type` and `location_id` for IOT-36's filters

### A bug the unit tests could not have caught
The first version of the `main.py` edit **silently did not apply** — the script
reported success because it asserted on `requirements.txt` but not on `main.py`.
Every unit test still passed, because they build their own engine and never
exercise startup. Booting the real image is what exposed it: `/health` returned
200 and the database had **zero tables**.

Two things came out of that: the wiring now uses `lifespan` rather than the
deprecated `@app.on_event("startup")`, and the "tables auto-created on startup"
criterion is verified against a running container rather than inferred.

## Out of scope, deliberately
`GET /users` still returns its stub. The models exist, but the table starts
empty, so switching it now would return `[]` and blank the frontend that
currently renders two rows. It moves across with the auth work that populates it.

## Note for IOT-35
`devices.location_id` and `devices.device_type` are both indexed, which is what
IOT-36's filters will want. The orphan question above is the one real decision
IOT-35 inherits.

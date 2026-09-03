# db-schema — Status: Implemented (IOT-34)

## Overview

backend-api uses **SQLite via SQLAlchemy 2.0**, configured by `DATABASE_URL`
(default `sqlite:////app/data/app.db` in the container). Models live in
`backend-api/app/models.py`; engine, session and schema creation in
`backend-api/app/db.py`.

## What these tables are — and are not

They are a **registry**, not a record of what happened.

storage-core owns the append-only hash chain, and it is the source of truth for
events. Nothing here may contradict it. So `devices` and `locations` describe
what is *registered*; anything about what a device *did* — last seen, reading
counts, whether it is active — is **derived by reading the ledger** (IOT-35) and
deliberately is not stored. A cached activity flag needs something to keep it
true and goes stale the moment that something fails.

## Tables

### `users`
| Column | Type | Notes |
|---|---|---|
| `id` | int PK | surrogate |
| `username` | str(64) | unique, indexed |
| `password_hash` | str(255), nullable | **unused — see below** |
| `role` | str(32) | defaults to `operator` |
| `created_at` | UTC datetime | |

> **No authentication is implemented.** `password_hash` exists because the
> original sketch called for it; nothing writes or checks it and no endpoint
> authenticates. The auth ticket must choose a real KDF (argon2 or bcrypt) and
> store only its output — never a plaintext password, never a bare SHA.

### `locations`
| Column | Type | Notes |
|---|---|---|
| `id` | str(64) PK | **matches the ledger's `location_id` verbatim** |
| `name` | str(128) | human-readable |
| `description` | text, nullable | |

### `settings`
| Column | Type | Notes |
|---|---|---|
| `id` | int PK | |
| `key` | str(128) | unique, indexed |
| `value` | text | |
| `updated_at` | UTC datetime | moves on update |

App-wide rather than per-location, which **resolves the open question** in the
earlier sketch. Per-location overrides would mean a nullable `location_id` and a
fallback lookup; easy to add when something needs it, pointless before.

### `devices`
| Column | Type | Notes |
|---|---|---|
| `device_id` | str(64) PK | **matches the ledger's `actor` verbatim** |
| `location_id` | str(64) FK → `locations.id`, nullable | indexed for IOT-36 filters |
| `device_type` | str(64) | indexed for IOT-36 filters |
| `registered_at` | UTC datetime | |

`location_id` is nullable so a device can be registered before its placement is
decided.

## Decision: natural string keys, not integer surrogates

`locations.id` and `devices.device_id` are the exact strings the firmware puts
in a block's `location_id` and `actor` (from its `secrets.h` — see
[`firmware.md`](firmware.md)). Correlating a registry row to ledger events is
therefore a direct key match with no translation table.

**Rejected:** integer surrogate PKs plus a unique slug — the more conventional
relational shape, and it would let a location be renamed without touching
firmware. It loses because the ledger is immutable: historical blocks can never
be rewritten to point at a new key, so the mapping layer would have to exist
forever and buy nothing.

**The cost, stated plainly:** renaming a location means re-flashing the devices
that report to it, and a typo in `secrets.h` produces events matching no
registered location. IOT-35 has to decide what to do with those orphans.

## Two portability details worth knowing

**Foreign keys are off by default in SQLite.** `app/db.py` registers a `connect`
listener issuing `PRAGMA foreign_keys=ON`. Without it a device could reference a
location that does not exist and nothing would complain — constraints would be
real in production and decorative in development.

**SQLite has no timezone type.** A column declared `DateTime(timezone=True)`
stores an aware value and hands back a *naive* one, while Postgres `TIMESTAMPTZ`
returns it aware. That divergence produces code that works in one environment
and breaks in the other. `app/db.py` defines a `UtcDateTime` type decorator that
normalises both directions — naive in is assumed UTC, naive out has UTC
attached, non-UTC input is converted — so the contract holds on every backend.
It is effectively a pass-through on Postgres.

## Schema creation, and the Postgres path

Tables are created at startup by `init_db()` via `Base.metadata.create_all()`.

`create_all` is **create-if-absent only**: it never alters or drops an existing
table, so a column added to a model will not appear on a database that already
has that table. That is acceptable while the schema is still moving and the data
is disposable. It is not acceptable once there is data worth keeping.

**Migrating to Postgres** is a `DATABASE_URL` change plus a driver
(`psycopg[binary]`) — the models use no SQLite-specific types, and the one
SQLite-only connect arg (`check_same_thread`) is already behind a URL check in
`_engine_kwargs`. The real work is introducing **Alembic** and generating a
baseline migration, which should happen at the same time; carrying `create_all`
into a database with real data is how schema drift starts.

## Open questions

- Final auth model (affects `users`, and whether `password_hash` survives as named)
- When Postgres + Alembic land — before or after the dashboard needs real data
- What IOT-35 does with ledger events whose `location_id`/`actor` match no registered row

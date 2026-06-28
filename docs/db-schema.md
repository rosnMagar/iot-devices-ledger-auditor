# db-schema — Status: Not Started

## Overview (TBD)

Rough, non-final sketch to anchor Phase 2 design. backend-api uses SQLite via SQLAlchemy (documented Postgres migration path).

## Stack / Decisions

Tentative tables:

- **users** — `id`, `username`, `password_hash`, `role`, `created_at`
- **locations** — `id`, `name`, `description`
- **settings** — `id`, `key`, `value` (app-wide config, e.g. alert thresholds)
- **devices** — `device_id`, `location_id` (FK), `device_type`, `registered_at` — registry for ESP32/IoT devices (needed once firmware moves past hardcoded config, see [`firmware.md`](firmware.md))

## Open Questions

- Final auth model (affects `users` schema)
- Whether `settings` is global or per-location
- Postgres migration timeline

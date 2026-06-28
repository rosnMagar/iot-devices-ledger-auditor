# IOT-34: backend-api — SQLAlchemy models (users/locations/settings/devices)

**Sprint:** sprint-04
**Story points:** 5
**Status:** To Do
**Depends on:** —

## Story
As a developer, I want persistent relational models so that users, locations, settings, and devices can be managed.

## Acceptance criteria
- [ ] SQLAlchemy configured with SQLite (`DATABASE_URL`), session/engine wiring
- [ ] Models: `users`, `locations`, `settings`, `devices` (`device_id`, `location_id` FK, `device_type`, `registered_at`)
- [ ] Tables auto-created on startup (or via init script)
- [ ] `docs/db-schema.md` updated to match the implemented schema

## Implementation notes
- See `docs/db-schema.md` for the tentative sketch; finalize here.
- Document a Postgres migration path; keep models DB-agnostic.

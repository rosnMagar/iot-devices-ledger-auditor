# IOT-71: sensors table + per-sensor reading payload

**Sprint:** sprint-05
**Story points:** 3
**Status:** Review
**Depends on:** —

## Story
As an operator, I want a device's sensors registered individually so that each one's readings can be tracked and drawn on its own.

## Acceptance criteria
- [x] `sensors` table: `sensor_id` PK, `device_id` FK, `sensor_type`, `unit`, `registered_at`
- [x] A device may have many sensors; deleting a device does not orphan rows
- [x] A device with no sensors is valid, not an error
- [x] `docs/db-schema.md` documents the table and the device relationship
- [x] Tests: relationship, cascade behaviour, duplicate `sensor_id` within a device

## Implementation notes
- ADR 0010. `sensor_id` is unique within a device, not globally — two boards may
  both call their probe `temp-0`.
- Registry only. Readings stay in the ledger; nothing here records a measurement,
  same rule as `devices` (see `docs/db-schema.md`).
- `unit` on the sensor row is what it is *expected* to report. The authoritative
  unit is the one in each reading, so a swapped probe shows up in the data.

## What was built
`Sensor` in `backend-api/app/models.py`, 10 tests in `tests/test_sensors.py`,
and a `sensors` section in `docs/db-schema.md`.

## Deviation from the acceptance criteria
The AC said `sensor_id` PK; the implementation notes said unique *within* a
device. Those conflict, and the notes are right — two boards may both call their
probe `temp-0`. The key is therefore **composite** (`device_id`, `sensor_id`),
which is also how a reading identifies itself: `actor` is the device,
`metadata.sensor_id` the sensor. A global `sensor_id` PK would have forced
firmware to invent globally unique ids for parts that have no such identity.

## Notes for review
- Cascade is set at both levels — ORM `delete-orphan` and DB `ON DELETE CASCADE`.
  SQLite enforces the latter only because `app/db.py` turns foreign keys on.
- `unit` on the row is the *expected* unit; the reading's own unit is
  authoritative (ADR 0010), so a swapped probe is visible rather than silent.
- `test_sensors_store_no_readings` pins the column set, mirroring the same guard
  on `devices`. If a later ticket adds a value column, that failing test is the
  prompt to justify it.
- Mutation tested: making `sensor_id` a global PK failed 3 tests; removing the
  cascade failed 1.

## Results
backend-api: 75 passed (was 65), ruff clean.

# IOT-71: sensors table + per-sensor reading payload

**Sprint:** sprint-05
**Story points:** 3
**Status:** To Do
**Depends on:** —

## Story
As an operator, I want a device's sensors registered individually so that each one's readings can be tracked and drawn on its own.

## Acceptance criteria
- [ ] `sensors` table: `sensor_id` PK, `device_id` FK, `sensor_type`, `unit`, `registered_at`
- [ ] A device may have many sensors; deleting a device does not orphan rows
- [ ] A device with no sensors is valid, not an error
- [ ] `docs/db-schema.md` documents the table and the device relationship
- [ ] Tests: relationship, cascade behaviour, duplicate `sensor_id` within a device

## Implementation notes
- ADR 0010. `sensor_id` is unique within a device, not globally — two boards may
  both call their probe `temp-0`.
- Registry only. Readings stay in the ledger; nothing here records a measurement,
  same rule as `devices` (see `docs/db-schema.md`).
- `unit` on the sensor row is what it is *expected* to report. The authoritative
  unit is the one in each reading, so a swapped probe shows up in the data.

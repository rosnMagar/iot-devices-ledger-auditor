# IOT-72: simulator emits per-sensor readings

**Sprint:** sprint-05
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-71

## Story
As a developer, I want the simulator to model multi-sensor devices so that the dashboard can be built against realistic data.

## Acceptance criteria
- [ ] Registers sensors per device, not just devices
- [ ] One event per sensor reading, payload per ADR 0010
- [ ] Sensor types with different intervals (a fast one and a slow one)
- [ ] Sensors fail independently — one `null` does not null the others
- [ ] Emits an accelerometer vector `[x, y, z]` so the shape is exercised
- [ ] Existing guards and idempotency preserved

## Implementation notes
- Revises `backend-api/tools/simulate_readings.py` from IOT-59. The guards, the
  mean-reverting drift and the stdin invocation all carry over unchanged; the
  payload builder and registration change.
- Independent failure is the point: the flat payload could not express it, which
  is part of why ADR 0009 was superseded.
- A camera sensor should emit `stream_online` / `motion_detected` events and
  **never** frame data (ADR 0011).

# IOT-72: simulator emits per-sensor readings

**Sprint:** sprint-05
**Story points:** 2
**Status:** Review
**Depends on:** IOT-71

## Story
As a developer, I want the simulator to model multi-sensor devices so that the dashboard can be built against realistic data.

## Acceptance criteria
- [x] Registers sensors per device, not just devices
- [x] One event per sensor reading, payload per ADR 0010
- [x] Sensor types with different intervals (a fast one and a slow one)
- [x] Sensors fail independently — one `null` does not null the others
- [x] Emits an accelerometer vector `[x, y, z]` so the shape is exercised
- [x] Existing guards and idempotency preserved

## Implementation notes
- Revises `backend-api/tools/simulate_readings.py` from IOT-59. The guards, the
  mean-reverting drift and the stdin invocation all carry over unchanged; the
  payload builder and registration change.
- Independent failure is the point: the flat payload could not express it, which
  is part of why ADR 0009 was superseded.
- A camera sensor should emit `stream_online` / `motion_detected` events and
  **never** frame data (ADR 0011).

## Notes for review
- **`CAMERA_EVENT`, not `SENSOR_READING`.** A camera has no value and no unit;
  forcing it into the reading shape would mean inventing a null measurement for
  every event. ADR 0011 was amended to pin this.
- Each sensor runs on **its own clock** — the accelerometer every 2s, humidity
  every 30s — so the loop is a due-time scheduler rather than one flat interval.
  `SIM_INTERVAL` is gone; `SIM_TICK` only sets how often the scheduler looks.
- `seq` is **per sensor**, not per device, so a gap points at the sensor that
  dropped readings.
- The fleet is deliberately uneven: a 2-sensor board, a 3-sensor board, a board
  with two thermometers, and a camera board. A uniform fleet would not exercise
  the panel.

## Verified end to end
Against real `storage-core` + `backend-api` containers:
- 10 sensors registered across 4 devices, including `temp-0` **and** `temp-1` on
  one board — the case the superseded flat payload could not express
- accelerometer wrote `[-0.34, -0.57, 9.2]` with `unit: m_s2`, gravity on z
- camera wrote `CAMERA_EVENT` / `stream_online` with no media in the payload
- cold-store 4.3°C against warehouse 20.6°C
- differing intervals visible in the block order: the accelerometer posted three
  times while humidity posted once
- `SIM_FAILURE_RATE=1.0` gave `value: null` per sensor with `seq` still advancing
- `GET /verify` valid over 13 blocks; re-run left 4 devices / 10 sensors

## Results
backend-api: 83 passed (was 75), ruff clean.

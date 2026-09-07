# IOT-73: expose sensors through the API

**Sprint:** sprint-05
**Story points:** 3
**Status:** Review
**Depends on:** IOT-71

## Story
As the dashboard, I want to know a device's sensors so that I can decide what to draw for it.

## Acceptance criteria
- [x] `GET /devices/{device_id}/sensors` returns the registered sensors
- [x] `GET /devices` includes a sensor summary per device (count and types)
- [x] Latest reading per sensor available without fetching a whole series
- [x] Unknown device is a 404 with a clear message
- [x] Sensors seen in the ledger but not registered are reported, not hidden
- [x] Tests for each of the above

## Implementation notes
- The orphan case is the same shape as IOT-35's `known_actors`: a typo in the
  firmware config produces readings belonging to no registered sensor. Surfacing
  it is how that typo gets found.
- "Latest reading per sensor" is what the panel needs on first paint; the full
  series is `GET /readings` (IOT-62).

## Notes for review
- `LedgerActivity` now tracks the latest reading per `(device_id, sensor_id)`
  beside `last_seen`, so the panel's first paint needs no series fetch.
- Only `SENSOR_READING` blocks are consumed as readings. `CAMERA_EVENT`s carry no
  value (ADR 0011) and pre-ADR-0010 blocks conform to nothing — both are skipped
  rather than coerced.
- Out-of-order delivery cannot rewind a latest reading, matching the existing
  rule for `last_seen`.
- Orphan sensors are listed with `registered: false`, the same treatment IOT-35
  gives orphan actors and for the same reason.

## A bug caught before it shipped
Splitting `_consume` accidentally left the `chain_length` cursor update inside
the new `_consume_reading`, where `payload` does not exist. ruff's F821 caught
it. Had it shipped, `_next_index` would never advance and every refresh would
rescan the chain from zero — a slow-burning version of IOT-56.

## Verified end to end
Against real `storage-core` + `backend-api` containers with the simulator:
- `esp32-03` returned `temp-0`, `temp-1` and `acce-0`, each with its own latest
  reading — the vector came back as `[-0.88, -1.01, 8.6]` unchanged
- a hand-posted `tpm-0` reading (a firmware typo) appeared with
  `registered: false` and `unregistered_count: 1`
- unknown device returned `404 {"detail": "no such device: 'nope'"}`
- `GET /devices` carried `sensor_count` and sorted `sensor_types` per device

## Results
backend-api: 93 passed (was 83), ruff clean.

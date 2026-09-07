# IOT-59: Sensor reading simulator

**Sprint:** sprint-05
**Story points:** 3
**Status:** Review
**Depends on:** IOT-58

## Story
As a developer without hardware, I want simulated devices posting readings so that the live feed and charts have something real to show.

## Acceptance criteria
- [x] Script posts `SENSOR_READING` events for N devices on an interval
- [x] Values drift plausibly rather than jumping randomly
- [x] Registers the matching `devices`/`locations` rows so the table shows them
- [x] Documented in `docs/local-setup.md`, clearly marked as a dev tool
- [x] Refuses to run against production unless explicitly forced

## Implementation notes
- Firmware (IOT-30…33) is blocked on hardware, so this is the only producer.
- The ledger is append-only: simulated readings are permanent. Hence the guard
  against pointing it at prod by accident.
- Plausible drift matters — a chart of uniform random noise proves nothing about
  whether the chart works.

## What was built
- `backend-api/tools/simulate_readings.py` — registers devices/locations, then
  posts `SENSOR_READING` events on an interval, payload per ADR 0009.
- `backend-api/tests/test_simulator.py` — 12 tests covering drift, payload shape
  and the guards.
- `docs/local-setup.md` — documented as a dev tool with the undo warning.

## Notes for review
- **Runs inside the backend-api container, piped over stdin.** It is the only
  place that can reach both the devices DB and storage-core, and stdin means the
  script never has to be baked into the production image.
- **Drift is a mean-reverting random walk**, not independent samples. Successive
  readings are close together the way real sensor data is — uniform noise would
  hide a chart that is silently redrawing instead of appending.
- Cold store devices really are colder (~4°C vs ~21°C), so a chart with several
  devices shows separated lines rather than one band.
- **Two guards.** `SIM_CONFIRM=1` is always required, because readings are
  permanent in a hash chain. A non-localhost `CORS_ORIGINS` is treated as
  production and additionally needs `SIM_FORCE=1`.
- Registration is idempotent; a re-run does not duplicate devices.
- A failed post is logged and the run continues — storage-core restarting
  mid-run should show up as a gap, which is what a real fleet would produce.

## Verified end to end
Against a real `storage-core` container (not a stub) plus `backend-api`:
- both guards refused as designed
- 9 readings appended; `GET /verify` returned `valid: true` over 10 blocks
- payload exactly `{"celsius": 21.58, "humidity_pct": 41.3, "seq": 1}`
- cold-store 3.2°C vs warehouse 21.6°C
- `SIM_FAILURE_RATE=1.0` produced `{"celsius": null, "humidity_pct": null, "seq": 1}`
  — keys present, values null, seq still advancing
- re-run left the device count at 3, not 6

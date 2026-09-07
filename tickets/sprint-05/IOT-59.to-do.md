# IOT-59: Sensor reading simulator

**Sprint:** sprint-05
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-58

## Story
As a developer without hardware, I want simulated devices posting readings so that the live feed and charts have something real to show.

## Acceptance criteria
- [ ] Script posts `SENSOR_READING` events for N devices on an interval
- [ ] Values drift plausibly rather than jumping randomly
- [ ] Registers the matching `devices`/`locations` rows so the table shows them
- [ ] Documented in `docs/local-setup.md`, clearly marked as a dev tool
- [ ] Refuses to run against production unless explicitly forced

## Implementation notes
- Firmware (IOT-30…33) is blocked on hardware, so this is the only producer.
- The ledger is append-only: simulated readings are permanent. Hence the guard
  against pointing it at prod by accident.
- Plausible drift matters — a chart of uniform random noise proves nothing about
  whether the chart works.

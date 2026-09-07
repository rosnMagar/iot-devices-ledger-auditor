# IOT-73: expose sensors through the API

**Sprint:** sprint-05
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-71

## Story
As the dashboard, I want to know a device's sensors so that I can decide what to draw for it.

## Acceptance criteria
- [ ] `GET /devices/{device_id}/sensors` returns the registered sensors
- [ ] `GET /devices` includes a sensor summary per device (count and types)
- [ ] Latest reading per sensor available without fetching a whole series
- [ ] Unknown device is a 404 with a clear message
- [ ] Sensors seen in the ledger but not registered are reported, not hidden
- [ ] Tests for each of the above

## Implementation notes
- The orphan case is the same shape as IOT-35's `known_actors`: a typo in the
  firmware config produces readings belonging to no registered sensor. Surfacing
  it is how that typo gets found.
- "Latest reading per sensor" is what the panel needs on first paint; the full
  series is `GET /readings` (IOT-62).

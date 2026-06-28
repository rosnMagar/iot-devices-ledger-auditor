# IOT-35: backend-api — define "active device" + derive `last_seen`

**Sprint:** sprint-04
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-34, IOT-22

## Story
As an operator, I want to know which devices are currently active so that I can monitor the fleet.

## Acceptance criteria
- [ ] A device's `last_seen` is derived from its most recent event in storage-core (`GET /blocks` filtered by `actor`/device)
- [ ] "Active" = `last_seen` within a configurable window (e.g. `ACTIVE_WINDOW_SECONDS`, default 300)
- [ ] Each device record exposes `status` (active/inactive) + `last_seen`
- [ ] Window is configurable via env

## Implementation notes
- Matching: events carry `actor`/`location_id`; map to `devices` rows.
- Consider caching `last_seen` to avoid scanning all blocks per request.

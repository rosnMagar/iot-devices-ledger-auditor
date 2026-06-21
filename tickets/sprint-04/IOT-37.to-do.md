# IOT-37: frontend — Devices table

**Sprint:** sprint-04
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-36

## Story
As an operator, I want a devices table so that I can see the fleet at a glance.

## Acceptance criteria
- [ ] Table columns: name, type, location, status, last seen
- [ ] Fetches from backend-api `GET /devices`
- [ ] Active vs inactive visually distinguished (e.g. status badge)
- [ ] Loading and empty states handled

## Implementation notes
- Reusable presentational component; data fetching separated from rendering.
- `last_seen` shown as relative time ("2m ago").

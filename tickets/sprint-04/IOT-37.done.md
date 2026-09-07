# IOT-37: frontend — Devices table

**Sprint:** sprint-04
**Story points:** 2
**Status:** Review
**Depends on:** IOT-36

## Story
As an operator, I want a devices table so that I can see the fleet at a glance.

## Acceptance criteria
- [x] Table columns: name, type, location, status, last seen
- [x] Fetches from backend-api `GET /devices`
- [x] Active vs inactive visually distinguished (e.g. status badge)
- [x] Loading and empty states handled

## Implementation notes
- Reusable presentational component; data fetching separated from rendering.
- `last_seen` shown as relative time ("2m ago").

## What was built
- `src/api.ts` — response types and `fetchDevices()`.
- `src/relativeTime.ts` — ISO timestamp to "2m ago"; `never` for a null.
- `src/DevicesTable.tsx` — presentational, props only. Owns the empty state.
- `src/useDevices.ts` — the fetch, aborted on unmount. Owns loading and errors.
- `App.tsx` — composes them; drops the Phase 0 raw JSON dumps.

## Notes for review
- Devices have no separate name column, so the Name column shows `device_id`;
  that is what the backend already sorts on for `sort=name`.
- Location shows `location_id`; `GET /devices` does not return location names.
- The status badge carries the word as well as the colour, so it does not rely
  on colour alone.
- `ledger_reachable: false` raises a banner — without it a stale ledger would
  render a dead fleet as healthy.
- A timestamp with no UTC offset is parsed as UTC, not local time. The API sends
  offsets; this only guards a regression there.

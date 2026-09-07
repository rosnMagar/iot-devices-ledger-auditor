# IOT-39: frontend — sortable columns + wire to query params

**Sprint:** sprint-04
**Story points:** 3
**Status:** Review
**Depends on:** IOT-38

## Story
As an operator, I want to sort the devices table and have filters/sort hit the backend so that large fleets stay responsive.

## Acceptance criteria
- [x] Clickable column headers sort by last seen / name / location, toggling asc/desc
- [x] Active filters + sort translate to `GET /devices` query params
- [x] URL reflects current filter/sort state (shareable/back-button friendly)
- [x] Refetch on filter/sort change with a loading indicator

## Implementation notes
- Single source of truth for query state → derive both the request and the URL.
- Debounce rapid changes if needed.

## What was built
- `src/query.ts` — `DeviceQuery`, `toSearchParams`, `fromSearchParams`, `toggleSort`.
- `src/useUrlQuery.ts` — query state kept in step with the address bar.
- `useDevices(apiBase, query)` — refetches on query change, keeps rows during it.
- `useFleetOptions` — one unfiltered read for the dropdowns and the fleet total.
- `DevicesTable` — sortable headers with `aria-sort` and a direction arrow.
- `filters.ts` — `applyFilters` removed; the backend does the filtering now.

## Notes for review
- Only `last_seen`, `name` and `location` are sortable — those are the keys the
  backend accepts. Type and Status render as plain headers rather than dead
  buttons, so nothing invites a click that does nothing.
- **A second, unfiltered request** backs the dropdowns. The filtered response
  only contains matches, so building options from it would let one filter empty
  the others and strand the operator. It also supplies the true total for
  "3 of 5 devices".
- Unknown values in a hand-edited URL fall back to defaults rather than being
  forwarded — the backend 400s on those, which would blank the page.
- Defaults are omitted from the URL, so the common case stays a bare address.
- No debounce: selects and header clicks are discrete, so one change is one
  request. Verified against the production build — a page load is 2 requests
  (table + options) and a sort change is exactly 1.

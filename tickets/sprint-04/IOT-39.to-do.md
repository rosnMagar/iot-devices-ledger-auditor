# IOT-39: frontend — sortable columns + wire to query params

**Sprint:** sprint-04
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-38

## Story
As an operator, I want to sort the devices table and have filters/sort hit the backend so that large fleets stay responsive.

## Acceptance criteria
- [ ] Clickable column headers sort by last seen / name / location, toggling asc/desc
- [ ] Active filters + sort translate to `GET /devices` query params
- [ ] URL reflects current filter/sort state (shareable/back-button friendly)
- [ ] Refetch on filter/sort change with a loading indicator

## Implementation notes
- Single source of truth for query state → derive both the request and the URL.
- Debounce rapid changes if needed.

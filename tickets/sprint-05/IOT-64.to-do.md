# IOT-64: Live-update the devices table

**Sprint:** sprint-05
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-61

## Story
As an operator, I want the devices table to update itself so that status is current without reloading.

## Acceptance criteria
- [ ] `last_seen` and `status` update from the stream, without a refetch
- [ ] Active-to-inactive still happens on time as the window elapses
- [ ] Filters and sort from IOT-38/39 keep working while updates arrive
- [ ] Falls back to the current fetch-on-load behaviour if the socket is down

## Implementation notes
- The table currently only updates on page reload.
- Sort order is server-side (IOT-39); a pushed update can make a row's position
  wrong. Decide whether to re-sort client-side or leave the row until the next
  fetch, and say which in the ticket notes.
- Status decays with time, not with events — a device going quiet produces no
  block, so a timer is still needed to flip it to inactive.

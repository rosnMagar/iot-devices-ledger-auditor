# IOT-64: Live-update the devices table

**Sprint:** sprint-05
**Story points:** 2
**Status:** Review
**Depends on:** IOT-61

## Story
As an operator, I want the devices table to update itself so that status is current without reloading.

## Acceptance criteria
- [x] `last_seen` and `status` update from the stream, without a refetch
- [x] Active-to-inactive still happens on time as the window elapses
- [x] Filters and sort from IOT-38/39 keep working while updates arrive
- [x] Falls back to the current fetch-on-load behaviour if the socket is down

## Implementation notes
- The table currently only updates on page reload.
- Sort order is server-side (IOT-39); a pushed update can make a row's position
  wrong. Decide whether to re-sort client-side or leave the row until the next
  fetch, and say which in the ticket notes.
- Status decays with time, not with events — a device going quiet produces no
  block, so a timer is still needed to flip it to inactive.

## Scope note: a requirement that fell through a gap
IOT-63 required "New readings from the WebSocket append without a refetch". When
IOT-63 was superseded by IOT-74, that criterion was not carried across, and
IOT-74 shipped without it. So live **charts** were unticketed.

Delivering IOT-64 as written would have produced a dashboard whose table said
"live" while every graph stayed frozen — the opposite of the point. Both are
included here.

## What was built
- `blockStream.ts` — pure merge rules: `applyFrame`, `statusFrom`, `streamUrlFrom`.
- `useBlockStream.ts` — the browser WebSocket client, with its own backoff, plus
  `useTicker`.
- `App.tsx` — layers live values over the fetched rows, feeds the panel, and
  shows a live/not-live indicator.
- 20 tests in `blockStream.test.ts`.

## Notes for review
- **Row positions are deliberately not re-sorted.** The sort is server-side
  (IOT-39); rows jumping under the cursor on every reading would be worse than a
  briefly stale ordering. The next fetch reorders. This was the open question in
  the ticket's implementation notes.
- **Status is recomputed on a 5s ticker**, not only on arrival. A device going
  quiet produces no block, so without the timer it would stay "active" for ever.
- Readings are de-duplicated on `(sensor, seq, timestamp)`: a reading can arrive
  on the socket that the `/readings` backfill also returned.
- The stream buffer is bounded at 500, matching the server's per-sensor history
  cap, so a tab left open overnight cannot grow without limit.
- `last_seen` never rewinds on out-of-order delivery, matching the backend rule.
- The client reconnects with its own backoff — a dashboard left open will
  outlive at least one backend restart.
- The socket being down is **not** an outage: everything still works from the
  fetch. But the indicator says so, because a dashboard that has silently
  stopped updating while looking live is worse than one that admits it.

## A gap found during verification
The chart appended live while the **gauge beside it stayed frozen** at the value
from selection time, still labelled with a stale "1m ago". The gauge reads
`sensor.latest`, which comes from the fetch and the stream never touched. Fixed
by deriving `latest` from the merged readings.

## Verified in a browser
Real `storage-core` + `backend-api` + a running simulator, watching without
touching anything:
- table showed "just now" and stayed `active` as readings arrived
- the accelerometer chart extended from 8:58pm past 9:01pm on its own
- `temp-0` gauge moved 21.47 to 21.52 with two new chart points, no interaction
- the "● live" indicator was green throughout

An earlier run appeared to show nothing updating. That was a broken harness —
`docker exec -d` with a stdin redirect detaches before the pipe attaches, so the
simulator never ran and no blocks were produced. Re-tested with it actually
running.

## Results
frontend: 98 passed (was 78), eslint and build clean.

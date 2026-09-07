# IOT-62: GET /readings time series

**Sprint:** sprint-05
**Story points:** 3
**Status:** Review
**Depends on:** IOT-71

## Story
As an operator, I want recent readings for a device so that a chart has history to draw before anything new arrives.

## Acceptance criteria
- [x] `GET /readings?device_id=&sensor_id=&since=` returns an ordered time series
- [x] Non-`SENSOR_READING` blocks and unparseable payloads are skipped, not fatal
- [x] Bounded response size with a documented cap
- [x] Invalid params return 400 with a clear message
- [x] Tests cover filtering, ordering, the cap, and malformed payloads

## Implementation notes
- A live stream alone gives an empty chart until the next reading lands. This is
  the backfill.
- Readings are per-sensor (ADR 0010), so filtering by `sensor_id` happens
  server-side — a multi-sensor fleet would otherwise ship everything to the browser.
- A vector `value` (accelerometer) and a `null` (failed read) must both survive
  the response unchanged rather than being coerced.
- Derived from the ledger, not stored — same principle as `last_seen` in IOT-35.
- Scanning the whole chain per request is O(n) and will not hold up. Acceptable
  for the MVP; note the ceiling in the docs rather than pretending it scales.

## Notes for review
- Served from a **bounded per-sensor ring buffer** in `LedgerActivity`
  (`READING_HISTORY_LIMIT`, default 500), not by re-scanning the chain per
  request. `LedgerActivity` already consumes every block incrementally, so
  appending to a deque is nearly free, and the cache rebuilds from the whole
  chain on process start because the cursor begins at 0. The ceiling is
  documented in `docs/backend-api.md` rather than pretended away.
- Omitting `sensor_id` returns every sensor interleaved by time, so one request
  backs a whole panel.
- Trimming takes the **newest** end. Returning the oldest points that happen to
  fit would give a chart the least useful data it could have.
- `CAMERA_EVENT`s never appear; they carry no value (ADR 0011).

## A usability trap found by a failing test
`since` was rejected as invalid for a *correct* ISO timestamp. An unencoded `+`
in a query string decodes to a space, so `...T11:55:00+00:00` arrives with its
offset mangled and `fromisoformat` refuses it. Returning 400 for a value the
caller got right is a confusing failure, so that one shape is now repaired — and
only that shape, since a space is also a legal ISO date/time separator and a
blanket replacement would corrupt real values.

## Verified end to end
Against real `storage-core` + `backend-api` containers with the simulator:
- `acce-0` returned a 6-point series at 2s spacing — the sensor's own interval —
  with vectors intact: `[-0.7, 0.41, 9.96] m_s2`
- whole-device query returned 11 readings across `acce-0`, `temp-0`, `temp-1`,
  correctly interleaved by time
- `limit=3` gave `truncated: true` and the most recent three
- `404` unknown device; `400` for `limit=0` and `since=yesterday`
- an unencoded `+` in `since` was accepted and filtered correctly

## Results
backend-api: 110 passed (was 93), ruff clean.

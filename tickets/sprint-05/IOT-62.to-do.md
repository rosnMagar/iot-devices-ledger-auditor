# IOT-62: GET /readings time series

**Sprint:** sprint-05
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-58

## Story
As an operator, I want recent readings for a device so that a chart has history to draw before anything new arrives.

## Acceptance criteria
- [ ] `GET /readings?device_id=&since=` returns an ordered time series
- [ ] Non-`SENSOR_READING` blocks and unparseable payloads are skipped, not fatal
- [ ] Bounded response size with a documented cap
- [ ] Invalid params return 400 with a clear message
- [ ] Tests cover filtering, ordering, the cap, and malformed payloads

## Implementation notes
- A live stream alone gives an empty chart until the next reading lands. This is
  the backfill.
- Derived from the ledger, not stored — same principle as `last_seen` in IOT-35.
- Scanning the whole chain per request is O(n) and will not hold up. Acceptable
  for the MVP; note the ceiling in the docs rather than pretending it scales.

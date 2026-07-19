# IOT-22: `GET /blocks?from&to` handler

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-20

## Story
As a consumer, I want to fetch a range of blocks so that the dashboard and auditor can read history.

## Acceptance criteria
- [ ] Query params `from` (default 0) and `to` (default latest), inclusive
- [ ] Returns `{ "blocks": [...], "chain_length": N }`
- [ ] Invalid range (from > to, out of bounds) → 400 via `ApiError` (InvalidRange)
- [ ] Used by backend-api feed and the Lambda's "last 50" (`from = len - 50`)

## Implementation notes
- Shared (read) lock; copy the slice into the response.
- Clamp `to` to chain length; validate `from <= to`; parse params with `std::stoull` guarded against exceptions (malformed → 400).

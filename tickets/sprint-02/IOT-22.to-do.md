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
- [ ] Invalid range (from > to, out of bounds) → 400 via `AppError::InvalidRange`
- [ ] Used by backend-api feed and the Lambda's "last 50" (`from = len - 50`)

## Implementation notes
- Read lock only; clone the slice into the response.
- Clamp `to` to chain length; validate `from <= to`.

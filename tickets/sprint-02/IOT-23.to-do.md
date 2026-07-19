# IOT-23: `GET /verify` + integration tests

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-21, IOT-22

## Story
As an auditor, I want a verify endpoint and HTTP-level tests so that chain integrity is checkable and the API is proven.

## Acceptance criteria
- [ ] `GET /verify` returns `{ valid, chain_length, checked_blocks, first_invalid_index, verified_at }`
- [ ] Integration tests (httplib::Client against a locally-bound test server) cover `/health`, `POST /events`, `GET /blocks`, `GET /verify`
- [ ] Test: POST an event → it appears in `GET /blocks` and chain still verifies
- [ ] `ctest` green

## Implementation notes
- Build the app in tests with a temp ledger path; bind to an ephemeral port on `127.0.0.1` and hit it with `httplib::Client`.
- `verified_at = now_rfc3339()`; reuse `VerifyResult` from IOT-12 for the body shape.
- Closes out the "simple REST" milestone — deployable even if sprint-03 slips.

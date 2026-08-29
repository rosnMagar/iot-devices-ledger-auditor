# IOT-23: `GET /verify` + integration tests

**Sprint:** sprint-02
**Story points:** 3
**Status:** Review
**Depends on:** IOT-21, IOT-22

## Story
As an auditor, I want a verify endpoint and HTTP-level tests so that chain integrity is checkable and the API is proven.

## Acceptance criteria
- [x] `GET /verify` returns `{ valid, chain_length, checked_blocks, first_invalid_index, verified_at }`
- [x] Integration tests (httplib::Client against a locally-bound test server) cover `/health`, `POST /events`, `GET /blocks`, `GET /verify`
- [x] Test: POST an event → it appears in `GET /blocks` and chain still verifies
- [x] `ctest` green

## Implementation notes
- Build the app in tests with a temp ledger path; bind to an ephemeral port on `127.0.0.1` and hit it with `httplib::Client`.
- `verified_at = now_rfc3339()`; reuse `VerifyResult` from IOT-12 for the body shape.
- Closes out the "simple REST" milestone — deployable even if sprint-03 slips.

## What was built

**`install_verify()`** in `storage-core/src/server.cpp`. Takes the shared lock,
calls `Chain::verify()`, serializes the `VerifyResult` plus `verified_at`.
`first_invalid_index` is always present and `null` on an intact chain, so
consumers can read it without first checking the key exists.

**A refactor to make the server testable.** `serve()` owned its `httplib::Server`
and blocked forever, which no test can drive. Route installation is now
`install_routes(httplib::Server&, AppState&)`, declared in `server.hpp` and
called by both `serve()` and the tests. `server.hpp` forward-declares
`httplib::Server` rather than including the ~10k-line header, so `main.cpp`
doesn't pay for it.

**`tests/test_server.cpp`** — a `TestServer` fixture starting a real server on a
real socket. `bind_to_any_port("127.0.0.1")` takes an OS-assigned port, so cases
never collide with each other or with a storage-core already on 8080. Each gets
its own temp ledger, removed on destruction. An optional `seed` hook writes the
ledger file before the server loads it, which is how the tamper case starts from
a chain the API itself could never produce.

## Verified

`ctest` 3/3. `test-server`: **10 cases, 97 assertions, all passing.**

| Case | Covers |
|---|---|
| `/health` | 200, `{"status":"ok"}` |
| `POST /events` | 201, index 1, 64-char hash, chains onto genesis not zeros |
| `POST /events` bad input | 400 on malformed JSON / missing field / empty `event_type`, and the chain is still length 1 afterwards |
| POST → `GET /blocks` → `/verify` | the returned block is byte-for-byte the one in the chain; chain still verifies |
| `GET /blocks` ranges | inclusive both ends, both defaults, `to` clamping, `chain_length` is the whole chain |
| `GET /blocks` bad ranges | 400 across all seven malformed inputs |
| `GET /verify` | full five-field shape, `first_invalid_index` null |
| `GET /verify` tampered | `valid: false`, `first_invalid_index: 1`, still HTTP 200 |
| CORS | header present |
| unknown route | 404 |

The tamper case exists because `first_invalid_index`'s non-null branch was
otherwise never exercised.

Also checked against the real binary: `/verify` returns
`{"chain_length":2,"checked_blocks":2,"first_invalid_index":null,"valid":true,"verified_at":"..."}`.

## Follow-ups
- `/verify` is O(n) under the shared lock, so verifying a large ledger delays
  writers for as long as it runs. Wants a snapshot-then-verify split before the
  chain gets big.
- The CI smoke test only exercises `/health`. Now that `/verify` exists it would
  be a better single check — it proves the ledger loaded and is intact, not just
  that the process is up.

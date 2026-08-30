# IOT-22: `GET /blocks?from&to` handler

**Sprint:** sprint-02
**Story points:** 3
**Status:** Done
**Depends on:** IOT-20

## Story
As a consumer, I want to fetch a range of blocks so that the dashboard and auditor can read history.

## Acceptance criteria
- [x] Query params `from` (default 0) and `to` (default latest), inclusive
- [x] Returns `{ "blocks": [...], "chain_length": N }`
- [x] Invalid range (from > to, out of bounds) → 400 via `ApiError` (InvalidRange)
- [x] Used by backend-api feed and the Lambda's "last 50" (`from = len - 50`)

## Implementation notes
- Shared (read) lock; copy the slice into the response.
- Clamp `to` to chain length; validate `from <= to`; parse params with `std::stoull` guarded against exceptions (malformed → 400).

## What was built

`parse_index()` and `install_blocks()` in `storage-core/src/server.cpp`, wired into
`serve()` after `install_events`.

**`to` clamps, `from` doesn't.** `to=9999` on a 4-block chain returns all 4 rather
than a 400: a caller asking for "everything from here on" shouldn't have to know
the length first, and the Lambda's `from = len - 50` walks off the end on a short
chain. An out-of-bounds `from` is still a 400 — because `to` is clamped to the
last index first, `from > to` catches it and no separate bounds check is needed.

**`std::stoull` is not trusted on its own.** It accepts `"12abc"` (returning 12)
and wraps `"-1"` into a huge positive number, either of which would turn a typo
into a silently wrong range instead of an error. The value is checked for digits
by hand first; the guarded `stoull` then only has "too big for u64" left to fail
on.

**The slice is copied under the lock and serialized after it.** A concurrent
append can reallocate the chain's vector, so holding references past the lock
would dangle.

`chain_length` is the full chain length, not the slice length — a caller paging
through history needs the total, and it is what makes the Lambda's `len - 50`
possible.

## Verified

Built clean (no warnings), `ctest` 2/2. Against a live server on a scratch ledger,
after 3 `POST /events`:

| Case | Result |
|---|---|
| no params | `[0,1,2,3]`, `chain_length` 4 |
| `from=1&to=2` | `[1,2]` (inclusive both ends) |
| `from=2` | `[2,3]` (`to` defaults to latest) |
| `to=1` | `[0,1]` (`from` defaults to 0) |
| `from=2&to=2` | `[2]` (single block) |
| `to=9999` | `[0,1,2,3]` (clamped) |
| `from=3&to=1` | 400 `invalid range: from=3 exceeds to=1` |
| `from=99` | 400 `invalid range: from=99 exceeds to=3` |
| `from=abc` / `-1` / empty / `12abc` | 400 `from must be a non-negative integer` |
| `to=99999999999999999999` | 400 `to is out of range` |

Blocks come back as full `Block` JSON (index, timestamp, event, prev_hash, hash);
hash linkage checked across all 4 returned blocks including genesis vs 64 zeros;
CORS header present; 4 lines on disk and the same 4 blocks after a restart.

## Follow-ups
- No pagination cap: `GET /blocks` on a large ledger serializes the whole chain
  into one response. Fine at demo scale, wants a max page size before the ledger
  grows.
- HTTP-level tests for this endpoint land in IOT-23.

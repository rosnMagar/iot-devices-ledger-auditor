# IOT-26: Refactor `POST /events` onto the write queue

**Sprint:** sprint-03
**Story points:** 3
**Status:** Review
**Depends on:** IOT-25

## Story
As a developer, I want the events handler to go through the writer thread so that the single-owner write path is actually used.

## Acceptance criteria
- [x] Handler builds a `WriteRequest`, takes `get_future()` before pushing, pushes onto the queue, and blocks on the future for the resulting `Block`
- [x] Returns 201 + the block; a closed queue or a broken promise → 500
- [x] The direct `std::unique_lock` append in the current handler is removed
- [x] The existing integration tests (IOT-23) pass unchanged — the HTTP contract does not move
- [x] Picked up the `main.cpp` wiring deferred from IOT-25: the thread is started before `serve()` and joined after `queue.close()`

## What was built
- **`POST /events`** no longer writes anything. It validates, builds a
  `WriteRequest`, takes the future *before* pushing, then blocks on it. The
  rollback-on-persist-failure block is gone — the writer persists before
  publishing, so there is no half-applied state left to undo.
- **`AppState::log` was removed.** The writer thread owns the handle for its
  whole lifetime, so `main()` now holds the `ofstream` as a local, uses it for
  the genesis append (before the thread starts, so still single-writer), and
  moves it in. This is the part worth keeping: "only one thread ever writes the
  ledger file" is now enforced by the types — a handler cannot append because it
  has nothing to append to — rather than by remembering not to.
- **`main.cpp`** starts one `WriteQueue` + writer thread for the process
  lifetime and shuts down in order: `serve()` returns → `queue.close()` →
  `join()`. Closing first is what makes `pop()` return `nullopt`; joining
  without it would hang forever.

## Deviation: 500, not 503, for a closed queue
A refused push means the server is shutting down, which is a 503 more than a
500. The ticket specified 500, so that is what it does — flagged rather than
changed, since moving a status code is a contract change and this ticket's whole
point was that the HTTP contract does not move.

## Known gap: no graceful shutdown on a signal
The shutdown path added to `main.cpp` is correct but, in practice, only runs
when `serve()` returns from a bind failure. Nothing installs a `SIGTERM`
handler, so `docker stop` kills the process outright and the writer never
drains. Confirmed by smoke test: after `pkill -TERM`, the "writer thread joined"
line never appears.

Data already on disk is safe (`append_block` flushes every block), but requests
sitting in the queue are lost and their clients see a reset connection. This is
pre-existing — `serve()` blocked forever before this ticket too — and is worth
its own ticket: a signal handler calling `srv.stop()` would make the ordered
shutdown here actually reachable.

## Testing
`tests/test_server.cpp` — 13 cases / 144 assertions (10 from IOT-23 unchanged,
3 new). The IOT-23 cases needed **no assertion edits**; only the fixture moved,
gaining a queue and a writer thread exactly as `main()` has.
- 6 clients × 5 concurrent POSTs: every index 1..30 handed out exactly once,
  `/verify` still valid, chain length 31. Results are collected under a mutex
  and asserted on the main thread — doctest's macros are not thread-safe.
- `POST /events` with a null `write_queue` → 500 `"writer unavailable"`, rather
  than a 201 for an event that was never written.
- `POST /events` after `queue.close()` → 500 rather than hanging, and `GET
  /blocks` still answers 200: a closed write path does not take reads down.
- `ctest` 5/5 green; `--repeat until-fail:15` green; clean under
  `-fsanitize=thread`.
- End-to-end against the real binary: 3 POSTs → 201, 4 lines on disk, restart
  reloads 4 blocks without duplicating genesis, a further POST appends a 5th and
  the chain still verifies.

## Implementation notes
- **Call `get_future()` before the request is pushed.** Once it's on the queue
  the writer thread may consume, fulfil, and destroy it before the handler gets
  another look at it; calling `get_future()` afterwards is a use-after-move race.
- `future.get()` re-throws whatever the writer set. A `StorageError` is left to
  the existing exception handler — `to_error_response` maps any non-`ApiError`
  to a 500 already, and the writer has logged the cause.
- Validation stays in the handler and still happens *before* anything is queued,
  so bad input never occupies a queue slot.
- The handler thread now blocks on the future instead of on a mutex. cpp-httplib
  is thread-per-connection so blocking is fine — and a full queue blocking
  handler threads is the intended backpressure.

## Rewritten for C++ (2026-08-29)
Was: handler `.await`s a `oneshot::Receiver` after sending on `write_tx`. Without
async, the handler blocks on `std::future::get()` instead. The Rust note about
"holding locks across `.await`" does not apply.

# IOT-25: `writer.hpp/cpp` — the writer thread

**Sprint:** sprint-03
**Story points:** 5
**Status:** Review
**Depends on:** IOT-24

## Story
As a developer, I want a single writer thread that owns the chain and the file handle so that appends are race-free and there is one place to fan out new blocks from.

## Acceptance criteria
- [x] `void writer_loop(WriteQueue&, AppState&, std::ofstream log)` owns the `Chain` and the log handle for its whole lifetime
- [x] Per request: compute the next block → `append_block` → publish to the readers' snapshot under a unique lock → fulfil the promise
- [ ] ~~Started once in `main.cpp` as a `std::thread`; joined on shutdown after `queue.close()`~~ — **deferred to IOT-26, deliberately.** See "Deviation" below.
- [x] A persist failure sets an exception on the promise and logs it — the thread keeps running and the failed block does not enter the chain
- [x] `ctest` green

## Deviation: main.cpp wiring moved to IOT-26
The thread cannot be started in `main.cpp` yet. `writer_loop` takes ownership of
the log handle, but `POST /events` still writes through `state.log` directly —
it doesn't route through the queue until IOT-26. Starting the thread now would
mean either moving `state.log` into the writer (breaking the live endpoint) or
opening a *second* append handle to the same file, which is worse: two
independently-buffered `ofstream`s on one file interleave partial lines and
corrupt the ledger.

So IOT-25 delivers the loop and its tests; IOT-26 starts the thread in the same
change that stops the handler from writing. That keeps `dev` working at every
commit instead of leaving a window where the ledger has two writers.

## Note: the rollback in the implementation notes is no longer needed
The original note said to drop the block from the chain if `append_block`
throws. The loop persists *before* publishing instead, so a failed write means
the block was never in the chain and there is nothing to undo. That removes the
failure mode the rollback existed to patch — a rollback that itself throws, or
is skipped, leaves a block in memory that a restart never sees. `POST /events`
keeps its rollback until IOT-26 retires that path.

## What was built
- `Chain::push_persisted(Block)` — appends an already-hashed, already-persisted
  block verbatim. `append()` can't be used because it does the hashing itself,
  which would happen under the write lock.
- `writer_loop` — pops until the queue closes; per request moves the promise out
  first so it is fulfilled exactly once on every path, hashes and persists
  outside the write lock, then takes the unique lock only for the vector push.
- Both `StorageError` and `...` are caught per request; the outer try/catch is
  the backstop that keeps an escaping exception from reaching the thread entry
  point and calling `std::terminate`.

## Testing
`tests/test_writer.cpp` — 16 cases / 325 assertions (10 from IOT-24, 6 new):
- persists, publishes and replies; 20 sequential appends stay hash-linked
- 6 threads × 10 concurrent submits: every index 1..60 handed out exactly once,
  chain still verifies
- persist failure — the fixture hands the thread an already-closed `ofstream`,
  so every `append_block` genuinely throws. Asserts the caller gets the
  `StorageError`, the chain stays at size 1, and a *second* request also gets
  its own failure, proving the thread survived rather than silently dying.
- requests queued before `close()` are still served, then the loop returns
- `ctest` green; `ctest --repeat until-fail:25` green; clean under
  `-fsanitize=thread` (no races reported)

## Implementation notes
- **Reply with `set_value`/`set_exception`, never by throwing.** An exception
  escaping `writer_loop` calls `std::terminate` and takes the process down. Any
  `StorageError` becomes `respond_to.set_exception(...)`, which re-throws inside
  the *handler* thread where the error mapper turns it into a 500.
- **Every request must get exactly one reply.** A `std::promise` destroyed
  without being set makes the waiting future throw `broken_promise`.
- Reads keep using the existing `std::shared_mutex` snapshot in `AppState`. The
  writer thread is the only unique-lock holder, and holds it only for the vector
  push — not across the file write.
- ADR 0003's rationale was amended for C++ under IOT-44 before this was built.

## Rewritten for C++ (2026-08-29)
Was: `writer_task` consuming an `mpsc::Receiver` in an async loop, updating
`Arc<RwLock<Vec<Block>>>`, replying via `oneshot`. `Arc<RwLock<...>>` maps onto
the `std::shared_mutex` + `Chain` already in `AppState`; the task becomes a plain
`std::thread`.

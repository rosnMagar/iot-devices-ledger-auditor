# IOT-25: `writer.hpp/cpp` — the writer thread

**Sprint:** sprint-03
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-24

## Story
As a developer, I want a single writer thread that owns the chain and the file handle so that appends are race-free and there is one place to fan out new blocks from.

## Acceptance criteria
- [ ] `void writer_loop(WriteQueue&, AppState&, std::ofstream log)` owns the `Chain` and the log handle for its whole lifetime
- [ ] Per request: compute the next block → `append_block` → publish to the readers' snapshot under a unique lock → fulfil the promise
- [ ] Started once in `main.cpp` as a `std::thread`; joined on shutdown after `queue.close()`
- [ ] A persist failure sets an exception on the promise and logs it — the thread keeps running and the failed block does not enter the chain
- [ ] `ctest` green

## Implementation notes
- **Reply with `set_value`/`set_exception`, never by throwing.** An exception
  escaping `writer_loop` calls `std::terminate` and takes the process down. Any
  `StorageError` becomes `respond_to.set_exception(...)`, which re-throws inside
  the *handler* thread where the error mapper turns it into a 500.
- **Every request must get exactly one reply.** A `std::promise` destroyed
  without being set makes the waiting future throw `broken_promise`. On
  `close()`, drain the remaining requests and set an exception on each.
- The rollback `POST /events` does today (IOT-21) moves here: if `append_block`
  throws, drop the block from the chain before replying, or every later block
  chains onto something a restart will never see.
- Reads keep using the existing `std::shared_mutex` snapshot in `AppState`. The
  writer thread is the only unique-lock holder, and holds it only for the vector
  push — not across the file write.
- Blocked on the ADR 0003 amendment (see IOT-44): the current rationale is
  Rust-specific and doesn't justify this design in C++.

## Rewritten for C++ (2026-08-29)
Was: `writer_task` consuming an `mpsc::Receiver` in an async loop, updating
`Arc<RwLock<Vec<Block>>>`, replying via `oneshot`. `Arc<RwLock<...>>` maps onto
the `std::shared_mutex` + `Chain` already in `AppState`; the task becomes a plain
`std::thread`.

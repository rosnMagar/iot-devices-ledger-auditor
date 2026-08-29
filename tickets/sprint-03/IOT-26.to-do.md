# IOT-26: Refactor `POST /events` onto the write queue

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-25

## Story
As a developer, I want the events handler to go through the writer thread so that the single-owner write path is actually used.

## Acceptance criteria
- [ ] Handler builds a `WriteRequest`, takes `get_future()` before pushing, pushes onto the queue, and blocks on the future for the resulting `Block`
- [ ] Returns 201 + the block; a closed queue or a broken promise → 500
- [ ] The direct `std::unique_lock` append in the current handler is removed
- [ ] The existing integration tests (IOT-23) pass unchanged — the HTTP contract does not move

## Implementation notes
- **Call `get_future()` before the request is pushed.** Once it's on the queue
  the writer thread may consume, fulfil, and destroy it before the handler gets
  another look at it; calling `get_future()` afterwards is a use-after-move race.
- `future.get()` re-throws whatever the writer set. Let a `StorageError` escape
  into the existing exception handler rather than catching it here — the mapper
  from IOT-18 already turns it into the right response.
- Validation (malformed JSON, empty `event_type`) stays in the handler and still
  happens *before* anything is queued, so bad input never occupies a queue slot.
- The handler thread now blocks on the future instead of on a mutex. Same
  latency, and cpp-httplib is thread-per-connection so blocking is fine — but
  a full queue now blocks handler threads, which is the intended backpressure.
- Because the tests from IOT-23 assert on HTTP behavior rather than internals,
  they are the regression check for this refactor. They should need no edits.

## Rewritten for C++ (2026-08-29)
Was: handler `.await`s a `oneshot::Receiver` after sending on `write_tx`. Without
async, the handler blocks on `std::future::get()` instead. The Rust note about
"holding locks across `.await`" does not apply.

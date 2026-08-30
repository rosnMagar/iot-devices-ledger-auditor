# IOT-24: `writer.hpp/cpp` — `WriteRequest` + bounded queue

**Sprint:** sprint-03
**Story points:** 3
**Status:** Review
**Depends on:** IOT-21

## Story
As a developer, I want a write-request queue so that appends are serialized through one owner instead of every handler taking a write lock.

## Acceptance criteria
- [x] `struct WriteRequest { EventPayload event; std::promise<Block> respond_to; }` in `include/writer.hpp`
- [x] `class WriteQueue` — `std::queue<WriteRequest>` guarded by a `std::mutex`, with `not_empty_`/`not_full_` condition variables and capacity 256
- [x] `push()` blocks while full; `pop()` blocks while empty; `close()` wakes both so shutdown can't deadlock
- [x] `AppState` carries the queue (a **pointer**, not a reference — see below)
- [x] Builds alongside the existing handlers (actually wired in IOT-26); `ctest` still green

## Implementation notes
- `WriteRequest` is **move-only** — `std::promise` is not copyable. The queue must
  move elements in and out (`std::move` on pop, `emplace` on push), and
  `std::queue::front()` returns a reference you have to move from before popping.
- Capacity 256 is deliberate backpressure: a flood of writes blocks the handler
  threads rather than growing an unbounded queue until the box runs out of memory.
- `close()` matters more in C++ than it did in the Rust sketch: a `pop()` parked
  on a condition variable never returns on its own, so `main()` can't join the
  writer thread without an explicit wake. Set a `closed_` flag, `notify_all()`,
  and have `pop()` return `std::nullopt` once closed and drained.
- Spurious wakeups are real: always wait with a predicate
  (`cv.wait(lock, [&]{ return ...; })`), never a bare `wait()`.
- See `docs/decisions/0003-ownership-based-concurrency.md` — **note its rationale
  is being revised**, see the C++ note in IOT-25.

## Rewritten for C++ (2026-08-29)
Was: `WriteRequest { respond_to: oneshot::Sender<Block> }` + `mpsc::channel(256)`
in `main.rs`. Tokio's `mpsc` is a bounded channel with async send/recv; the C++
equivalent is a mutex + condition-variable queue, and `oneshot::Sender<T>` maps
onto `std::promise<T>` / `std::future<T>`.


## What was built
`storage-core/include/writer.hpp` + `src/writer.cpp`: `WriteRequest` and
`WriteQueue`, compiled into `storage-core` and `test-server`. Nothing uses the
queue yet — `POST /events` keeps its `unique_lock` until IOT-26.

## Two deviations from the criteria, both deliberate

**`AppState` holds `WriteQueue*`, not `WriteQueue&`.** A reference member would
force every existing `AppState` construction — including the `TestServer` fixture
from IOT-23 — to supply a queue before anything uses one. A null pointer means
"no writer thread", which is exactly the state between now and IOT-26 and keeps
this ticket's own criterion ("builds alongside the existing handlers") satisfiable.

**`push` takes `WriteRequest&&`, not by value.** This one was found by a test
rather than by design, and it matters for IOT-26.

## The bug the tests caught
`push(WriteRequest request)` — by value — moves the caller's request at the call
site regardless of the outcome. So a *refused* push (closed queue) still stripped
the promise, and the caller's future threw
`std::future_error: No associated state`.

That breaks the contract IOT-26 depends on: the handler needs to fail the request
itself when the queue refuses it. If the promise is already gone, the waiting
future reports `broken_promise` instead of a clean 500. Fixed by taking an rvalue
reference and moving only once acceptance is certain.

## Verified
`ctest` **5/5**. `test-writer`: **10 cases, 52 assertions**, and green under
`ctest --repeat until-fail:15` — the threaded cases are not flaky.

| Case | Covers |
|---|---|
| push/pop roundtrip | the promise survives two moves and still drives the original future |
| FIFO | order preserved across five requests |
| full queue | producer blocks at capacity, resumes when a slot frees |
| empty queue | consumer blocks, wakes on push |
| `close()` wakes `pop` | the blocked writer thread can exit — otherwise `main()` can't join it |
| `close()` wakes `push` | blocked producer returns false |
| drain on close | already-accepted requests are still served, not dropped |
| push after close | refused, and the caller's promise still works |
| idempotent close | second call is harmless |
| 8 producers / 1 consumer | 400 requests through a capacity-16 queue, none lost |

The blocking tests use a sleep only as a *negative* check — asserting nothing has
happened yet — so it cannot produce a false pass the way sleeping to wait for
something can. Every positive wait is on a future with a timeout.

## Follow-up
`kDefaultCapacity` is 256 as specified, but nothing yet reports queue depth. Once
IOT-26 routes writes through it, a saturated queue will show up only as latency.
Worth exposing `size()` on `/health` or as a log line when it crosses a threshold.

# IOT-24: `writer.hpp/cpp` — `WriteRequest` + bounded queue

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-21

## Story
As a developer, I want a write-request queue so that appends are serialized through one owner instead of every handler taking a write lock.

## Acceptance criteria
- [ ] `struct WriteRequest { EventPayload event; std::promise<Block> respond_to; }` in `include/writer.hpp`
- [ ] `class WriteQueue` — `std::queue<WriteRequest>` guarded by a `std::mutex`, with `not_empty_`/`not_full_` condition variables and capacity 256
- [ ] `push()` blocks while full; `pop()` blocks while empty; `close()` wakes both so shutdown can't deadlock
- [ ] `AppState` carries a reference to the queue
- [ ] Builds alongside the existing handlers (actually wired in IOT-26); `ctest` still green

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

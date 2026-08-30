# Weekend 4 — Ring Buffer + WebSocket

Status: Not started

## Goal

Build a bounded producer/consumer queue and a writer thread that exclusively owns the `Chain` and log handle, then refactor `POST /events` to go through it. Add a block broadcaster (a per-subscriber ring buffer) and a WebSocket endpoint that streams new blocks. Capstone integration test: start the servers on ephemeral ports, connect a WebSocket client, POST an event, assert the client receives the matching block. "queue → hash chain → persist → broadcast" end to end.

Note that the writer thread is **not** a correctness fix in C++ — the `std::unique_lock` in `POST /events` is already correct. See the 2026-08-30 amendment to [`../decisions/0003-ownership-based-concurrency.md`](../decisions/0003-ownership-based-concurrency.md) for what does justify it.

## Concepts you need (read/skim before starting)

- **`std::condition_variable`** — the `wait(lock, predicate)` form and why the bare `wait()` is wrong (spurious wakeups). This is the core primitive for the bounded queue.
- **`std::promise` / `std::future`** — how a handler thread gets a value back from the writer thread, and what `set_exception` and `broken_promise` mean.
- **Move-only types in containers** — `WriteRequest` holds a `std::promise`, so it can't be copied; `std::queue::front()` gives a reference you must move from before popping.
- **Thread shutdown** — a thread parked in `cv.wait()` never returns on its own. A `close()` flag plus `notify_all()` is what lets `main()` join it.
- **WebSocket basics** — the HTTP Upgrade handshake and text framing, enough to use the vendored library (IOT-45) rather than implement it.

Deliberately *not* on this list: async/await, event loops, and thread pools. cpp-httplib is thread-per-connection and one thread per WebSocket connection is fine at this scale.

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD

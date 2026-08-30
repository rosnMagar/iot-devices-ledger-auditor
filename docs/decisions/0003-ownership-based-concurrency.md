# 0003 — Ownership-based concurrency for the write path

Status: Accepted (amended 2026-08-30 for the C++ migration — see the amendment below)

## Context

`POST /events` handlers run concurrently (one per incoming request) but must serialize appends to the chain and the log file — two concurrent writers must not race on `prev_hash`/`index` or interleave file writes.

## Decision

Instead of wrapping the chain/file handle in a `Mutex`, a single `writer_task` (spawned once in `main.rs`) owns the `Chain` and file handle exclusively. HTTP handlers send a `WriteRequest { event, respond_to: oneshot::Sender<Block> }` over an `mpsc::channel<WriteRequest>(256)` and await the `oneshot` for the resulting `Block`. The writer task computes the next block, appends to the log, updates a shared `Arc<RwLock<Vec<Block>>>` for reads, broadcasts the new block, and replies via the oneshot.

## Consequences

- No lock contention or risk of holding a `Mutex` across `.await` points on the write path — a common Rust async pitfall.
- Demonstrates a core Tokio idiom ("ownership-based concurrency": give one task exclusive ownership instead of sharing + locking).
- Reads (`GET /blocks`, `GET /verify`) go through a separate `RwLock<Vec<Block>>`, kept in sync by the writer task — cheap, non-blocking reads with near-zero write contention.
- Introduces a bounded channel (256) — backpressure under heavy write load is a deliberate, documented tradeoff (not expected to matter at this project's scale).

---

## Amendment — 2026-08-30 (C++ migration)

Everything above describes the original Rust/Tokio design and is accurate for it.
The project has since migrated to C++ with cpp-httplib, and **two of the four
consequences no longer apply**:

- *"No risk of holding a `Mutex` across `.await` points — a common Rust async
  pitfall"* — cpp-httplib is thread-per-connection. There is no async runtime and
  no `.await`, so the pitfall does not exist here.
- *"Demonstrates a core Tokio idiom"* — there is no Tokio.

**The writer thread is therefore not a correctness fix in C++.** The
`std::unique_lock` that `POST /events` takes today (IOT-21) already serialises
appends correctly: one writer at a time, chain and log file updated together
under the same lock. Someone reading this ADR before IOT-25 could reasonably
conclude the current code is racy. It is not.

### What still justifies the design

- **Explicit backpressure.** A bounded queue (256) makes the limit visible and
  tunable. A mutex gives you an unbounded implicit queue of blocked threads
  instead, which fails less predictably under load.
- **One fan-out point.** The broadcaster (IOT-27) needs a single place that sees
  every append in order. With N handler threads appending under a lock, ordering
  across subscribers gets fiddly; with one writer it is free.
- **The learning goal.** Weekend 4 exists to build a producer/consumer queue with
  a condition variable and a thread that owns its data. That is the point of the
  exercise, and it is a legitimate reason on a project that is partly about
  learning C++.

### What this means for sequencing

IOT-24 through IOT-27 remain worth doing, on the reasons above rather than the
ones originally written. If Weekend 4 is ever cut for time, **the broadcaster
(IOT-27) is the part the live feed actually requires** — it can be built directly
on the existing `shared_mutex` without the queue and writer thread.

# 0003 — Ownership-based concurrency for the write path

Status: Accepted

## Context

`POST /events` handlers run concurrently (one per incoming request) but must serialize appends to the chain and the log file — two concurrent writers must not race on `prev_hash`/`index` or interleave file writes.

## Decision

Instead of wrapping the chain/file handle in a `Mutex`, a single `writer_task` (spawned once in `main.rs`) owns the `Chain` and file handle exclusively. HTTP handlers send a `WriteRequest { event, respond_to: oneshot::Sender<Block> }` over an `mpsc::channel<WriteRequest>(256)` and await the `oneshot` for the resulting `Block`. The writer task computes the next block, appends to the log, updates a shared `Arc<RwLock<Vec<Block>>>` for reads, broadcasts the new block, and replies via the oneshot.

## Consequences

- No lock contention or risk of holding a `Mutex` across `.await` points on the write path — a common Rust async pitfall.
- Demonstrates a core Tokio idiom ("ownership-based concurrency": give one task exclusive ownership instead of sharing + locking).
- Reads (`GET /blocks`, `GET /verify`) go through a separate `RwLock<Vec<Block>>`, kept in sync by the writer task — cheap, non-blocking reads with near-zero write contention.
- Introduces a bounded channel (256) — backpressure under heavy write load is a deliberate, documented tradeoff (not expected to matter at this project's scale).

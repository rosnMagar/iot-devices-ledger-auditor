# IOT-25: `writer.rs` — `writer_task`

**Sprint:** sprint-03
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-24

## Story
As a developer, I want a single writer task that owns chain + file so that writes are race-free without a write-path mutex.

## Acceptance criteria
- [ ] `writer_task` owns the `Chain` + file handle exclusively
- [ ] Per request: compute block → `append_block` → update `Arc<RwLock<Vec<Block>>>` → reply via oneshot
- [ ] Spawned once in `main.rs`; consumes the mpsc receiver in a loop
- [ ] Persist failure is logged and reported back (no task crash)

## Implementation notes
- The reader `Arc<RwLock<Vec<Block>>>` is updated by the task after each append.
- Broadcast emission added in IOT-27.
- See concurrency diagram in `docs/storage-core/overview.md`.

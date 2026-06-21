# IOT-24: `writer.rs` — `WriteRequest` + mpsc channel

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-21

## Story
As a developer, I want a write-request channel so that appends are serialized through one owner instead of locks.

## Acceptance criteria
- [ ] `WriteRequest { event: EventPayload, respond_to: oneshot::Sender<Block> }`
- [ ] `mpsc::channel::<WriteRequest>(256)` created in `main.rs`
- [ ] `AppState` carries `write_tx: mpsc::Sender<WriteRequest>`
- [ ] Compiles alongside existing handlers (wired in IOT-26)

## Implementation notes
- See `docs/decisions/0003-ownership-based-concurrency.md`.
- Bounded channel (256) = deliberate backpressure.

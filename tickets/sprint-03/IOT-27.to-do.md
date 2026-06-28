# IOT-27: `broadcast::Sender<Block>` wiring

**Sprint:** sprint-03
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-25

## Story
As a developer, I want new blocks broadcast so that WebSocket clients can receive live updates.

## Acceptance criteria
- [ ] `broadcast::channel::<Block>(64)` created in `main.rs`
- [ ] `AppState` carries `broadcast_tx: broadcast::Sender<Block>`
- [ ] `writer_task` broadcasts each newly appended block
- [ ] A late subscriber receives only blocks appended after it subscribed

## Implementation notes
- Ignore `SendError` when there are no subscribers (expected).
- Capacity 64; lag handling is the WS handler's job (IOT-28).

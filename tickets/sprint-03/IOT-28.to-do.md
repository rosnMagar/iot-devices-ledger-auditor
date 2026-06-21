# IOT-28: `ws.rs` — `/ws/blocks` handler (handle Lagged)

**Sprint:** sprint-03
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-27

## Story
As a dashboard, I want a WebSocket stream of new blocks so that the live feed updates without polling.

## Acceptance criteria
- [ ] `GET /ws/blocks` upgrades and subscribes to the broadcast channel
- [ ] Each new `Block` is forwarded as JSON text
- [ ] `RecvError::Lagged` is handled by skipping ahead, not crashing
- [ ] Client disconnect cleans up the subscription

## Implementation notes
- Use axum's `WebSocketUpgrade`; loop on `broadcast::Receiver::recv()`.
- See axum WebSocket example + `docs/storage-core/weekend-4-ring-buffer-ws.md`.

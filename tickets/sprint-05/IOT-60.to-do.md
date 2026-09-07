# IOT-60: backend-api consumes the storage-core WebSocket

**Sprint:** sprint-05
**Story points:** 5
**Status:** To Do
**Depends on:** —

## Story
As backend-api, I want a persistent subscription to the ledger's block feed so that new blocks arrive without polling.

## Acceptance criteria
- [ ] Background task started in `lifespan` connects to `ws://storage-core:8081/blocks`
- [ ] Reconnects with backoff when the connection drops, without leaking tasks
- [ ] Handles the `{"type":"lagged","dropped":N}` notice rather than parsing it as a block
- [ ] Survives storage-core being down at startup (retries, does not crash the app)
- [ ] Shuts down cleanly on app shutdown
- [ ] Tests cover connect, drop/reconnect, lag notice, and shutdown

## Implementation notes
- storage-core has broadcast this since IOT-27/28 and nothing has ever consumed
  it — this is the first real client of that work.
- Frames are bare block JSON; lag notices are a different shape (see `src/ws.cpp`).
- The upstream half only. Fanout to browsers is IOT-61.
- Reconnection is the fiddly part: a tight reconnect loop against a dead
  storage-core must not spin.

# IOT-28: `ws.hpp/cpp` — `/blocks` WebSocket handler (handle lag)

**Sprint:** sprint-03
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-27, IOT-45

## Story
As a dashboard, I want a WebSocket stream of new blocks so that the live feed updates without polling.

## Acceptance criteria
- [ ] The WS server listens on `WS_BIND_ADDR` (default `0.0.0.0:8081`) and accepts connections at `/blocks`
- [ ] On connect, the connection takes a `Broadcaster::subscribe()` handle
- [ ] Each new `Block` is sent as a JSON text frame
- [ ] A subscription that dropped blocks (IOT-27's `lagged_` counter) sends the client a `{"type":"lagged","dropped":N}` frame instead of silently skipping
- [ ] Client disconnect deregisters the subscription — no leak, no write to a dead socket
- [ ] The server survives a client that connects and immediately disconnects, and a client that never reads

## Implementation notes
- **Lifetime is the hard part here, not the protocol.** A client can vanish at
  any moment, including while the writer thread is mid-`publish()`. Hold the
  subscription as a `shared_ptr` and deregister in its destructor so a
  disconnect during fan-out can't dangle.
- **Never let a slow client block the writer.** IOT-27 already guarantees this
  by dropping oldest into a bounded buffer; this handler must not reintroduce
  the coupling by, say, doing a blocking send while holding the subscriber list
  lock.
- The lag frame is the C++ equivalent of Tokio's `RecvError::Lagged`. Telling
  the client it missed blocks matters: a dashboard that silently skips blocks
  shows a chain with holes in it and no indication anything is wrong. The client
  can refetch via `GET /blocks?from=` to backfill.
- One thread per WS connection is fine at this scale. Don't build an event loop.
- `main.cpp` now starts three things: the writer thread, the WS server, and the
  HTTP server. Shutdown order matters — stop accepting, close subscriptions,
  then `queue.close()` and join the writer.

## Rewritten for C++ (2026-08-29)
Was: axum `WebSocketUpgrade` on `GET /ws/blocks`, looping on
`broadcast::Receiver::recv()` and matching `RecvError::Lagged`. cpp-httplib has
no WebSocket support at all, so the endpoint moves to a **separate listener on
port 8081** (see IOT-45) and the broadcast/lag machinery is the hand-built one
from IOT-27.

# IOT-28: `ws.hpp/cpp` — `/blocks` WebSocket handler (handle lag)

**Sprint:** sprint-03
**Story points:** 5
**Status:** Review
**Depends on:** IOT-27, IOT-45

## Story
As a dashboard, I want a WebSocket stream of new blocks so that the live feed updates without polling.

## Acceptance criteria
- [x] The WS server listens on `WS_BIND_ADDR` (default `0.0.0.0:8081`) and accepts connections at `/blocks`
- [x] On connect, the connection takes a `Broadcaster::subscribe()` handle
- [x] Each new `Block` is sent as a JSON text frame
- [x] A subscription that dropped blocks (IOT-27's `lagged_` counter) sends the client a `{"type":"lagged","dropped":N}` frame instead of silently skipping
- [x] Client disconnect deregisters the subscription — no leak, no write to a dead socket
- [x] The server survives a client that connects and immediately disconnects, and a client that never reads

## What was built
`WsServer` (pimpl, so the IXWebSocket headers stay out of everything that
includes `ws.hpp`) with `start()` / `stop()` / `connection_count()`.

**Three threads per connection, and the reason matters.** IXWebSocket already
runs an accept thread plus one thread per connection. Each connection gets a
third here — a *pump* that blocks on `Subscription::next()` and writes frames.

The pump has to be separate because the connection's own thread sits inside
`WebSocket::run()` dispatching callbacks, and that is also how a disconnect is
noticed. Blocking it on `next()` would mean a client that vanishes while no
blocks are being published is never detected, and its subscription leaks until
shutdown.

**Lifetime.** Registration uses `setOnConnectionCallback`, not
`setOnClientMessageCallback`, because it is the one that hands over a
`std::weak_ptr<ix::WebSocket>` (`IXWebSocketServer.h:25`). The pump holds that
weak_ptr and re-locks it before every send, so a client disappearing mid-stream
ends the loop rather than writing to a dead socket. Holding a `shared_ptr`
instead would keep the WebSocket alive past the connection it belongs to.

Verified in the vendored source before relying on it: `send()` appends under
`_txbufMutex` (`IXWebSocketTransport.cpp:871`), so sending from the pump thread
while the connection thread runs is safe.

**Shutdown.** `stop()` stops accepting and closes the sockets, which makes each
connection thread wind down and fire `on_close`, which closes its subscription
(waking the pump out of `next()`) and joins it. A second sweep then catches
anything that never delivered a Close — an aborted connection, or one closed
before it was fully open. `on_close` moves the entry out from under the map lock
before closing and joining, so one slow teardown does not block every other
connection's open/close.

`main.cpp` now starts three things. A WS bind failure is fatal rather than
warned about: coming up with the REST API healthy and the feed silently missing
is exactly the failure shape IOT-49/51/52 were about.

## Wire format
Two JSON text frame shapes:
- a **block**, serialised exactly as `GET /blocks` serialises it, so a client
  uses one parser for history and live updates;
- a **lag notice**, `{"type":"lagged","dropped":N}`, sent immediately *before*
  the block that follows the gap, so the client can attribute the missing range.

A client distinguishes them by the `type` field, which a block never has. That
is asserted in the tests so it cannot silently stop being true.

Considered and rejected: wrapping blocks as `{"type":"block", ...}` for
symmetry. It is tidier, but it would make the live frame a different shape from
the REST payload and cost the shared parser — the more valuable property.

## Testing
`tests/test_ws.cpp` — 11 cases / 121 assertions, driving **real IXWebSocket
clients against a real listener**:
- a connected client receives blocks as JSON frames, in order, with no `type`
- two clients each receive every block; a late client sees only what follows
- **a lagging client is told it missed blocks** — 400 blocks published faster
  than the socket drains; verified the notices carry real counts (observed 1, 3,
  2, 3, 2 …), not a vacuously-passing assertion
- a connection to an unknown path is refused and never subscribes
- ten connect-then-immediately-disconnect clients leak nothing, and the listener
  still serves a survivor afterwards
- **a client that never reads does not stop the publisher** — 2000 blocks into a
  stalled socket completes in well under 5s
- `stop()` closes connected clients and is idempotent
- client→server messages are ignored; the connection keeps working

`ctest` 7/7 green; `--repeat until-fail:15` on the WS suite green (these are
socket tests with real timing, so repetition is the check that matters); clean
under `-fsanitize=thread`, including the vendored library's threads against our
pump threads.

**End-to-end against the real binary:** started storage-core, connected a
standalone WS client, `POST`ed two events → both returned 201 and both blocks
arrived as frames, with block 2's `prev_hash` equal to block 1's `hash` — the
chain is intact across the live feed. Server log shows the listener binding,
`ws client connected (1 total)`, and `ws client disconnected`.

## Note for IOT-29: no ephemeral ports
`SocketServer::getPort()` (`IXSocketServer.cpp:551`) just returns the port it was
configured with — it does **not** resolve an OS-assigned port after binding, the
way httplib's `bind_to_any_port` does. A test cannot ask for port 0 and be told
what it got. `tests/test_ws.cpp` walks up from a base port until a bind succeeds;
IOT-29 should do the same rather than hard-coding one.

## Implementation notes
- **Lifetime is the hard part here, not the protocol.** Handled with the
  weak_ptr registration above.
- **Never let a slow client block the writer.** IOT-27 guarantees it by dropping
  oldest into a bounded buffer; this handler does not reintroduce the coupling —
  no send happens while any shared lock is held, and the pump is the only thing
  that ever blocks.
- One thread per WS connection is fine at this scale. No event loop.
- Shutdown order in `main.cpp`: stop accepting (`ws.stop()`), then
  `queue.close()`, join the writer, then `broadcaster.close_all()`.

## Known gap (IOT-54)
This shutdown path still only runs when `serve()` returns. There is no `SIGTERM`
handler, so `docker stop` kills the process and none of it executes — now with a
second listener and per-connection threads to leave behind. Filed as IOT-54.

## Rewritten for C++ (2026-08-29)
Was: axum `WebSocketUpgrade` on `GET /ws/blocks`, looping on
`broadcast::Receiver::recv()` and matching `RecvError::Lagged`. cpp-httplib has
no WebSocket support at all, so the endpoint moves to a **separate listener on
port 8081** (see IOT-45) and the broadcast/lag machinery is the hand-built one
from IOT-27.

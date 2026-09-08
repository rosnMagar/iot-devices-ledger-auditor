# IOT-60: backend-api consumes the storage-core WebSocket

**Sprint:** sprint-05
**Story points:** 5
**Status:** Review
**Depends on:** —

## Story
As backend-api, I want a persistent subscription to the ledger's block feed so that new blocks arrive without polling.

## Acceptance criteria
- [x] Background task started in `lifespan` connects to `ws://storage-core:8081/blocks`
- [x] Reconnects with backoff when the connection drops, without leaking tasks
- [x] Handles the `{"type":"lagged","dropped":N}` notice rather than parsing it as a block
- [x] Survives storage-core being down at startup (retries, does not crash the app)
- [x] Shuts down cleanly on app shutdown
- [x] Tests cover connect, drop/reconnect, lag notice, and shutdown

## Implementation notes
- storage-core has broadcast this since IOT-27/28 and nothing has ever consumed
  it — this is the first real client of that work.
- Frames are bare block JSON; lag notices are a different shape (see `src/ws.cpp`).
- The upstream half only. Fanout to browsers is IOT-61.
- Reconnection is the fiddly part: a tight reconnect loop against a dead
  storage-core must not spin.

## What was built
- `backend-api/app/blockfeed.py` — `BlockFeed`: one upstream subscription,
  reconnect with backoff, listener dispatch, clean shutdown.
- Started and stopped in `lifespan`; `GET /feed/status` exposes its counters.
- `STORAGE_CORE_WS_URL` in `docker-compose.yml` and `.env.example`.
- 9 tests in `tests/test_blockfeed.py`, driven against a **real local WebSocket
  server**, not a mock.

## Notes for review
- **`websockets` is already installed** — `uvicorn[standard]` pulls it in, so
  nothing new was installed. It is **not declared in `requirements.txt`**, which
  means a transitive extra is holding up a load-bearing feature. Declaring it is
  a one-line change and needs a decision.
- Backoff resets only after a *successful* connect. Resetting per attempt would
  turn a connect/drop loop into a hot loop.
- Jitter on the sleep so several backends do not reconnect in lockstep after a
  storage-core restart.
- `stop()` awaits the cancelled task rather than only cancelling it; an
  un-awaited task leaks and asyncio complains at interpreter shutdown.
- `/feed/status` exposes counters, not just a flag: "connected right now" hides
  a flapping feed, which looks healthy on any single check.

## Verified against real storage-core
The `storage-core:iot45` image predates the WebSocket work (it logs no 8081
listener), so storage-core was **built from current source** for this. Its log
line `websocket listening on 0.0.0.0:8081/blocks` confirms the target.

- Three `POST /events` produced three blocks delivered live over the socket,
  metadata intact — the first time the IOT-27/28 broadcaster has had a consumer.
- Cold start with storage-core down: retried at 0.5s, 1s, 2s, 4s, then connected
  within a second of the server appearing.
- Killed mid-connection: the drop was detected in **under a second** ("no close
  frame received or sent"), `connected` went False immediately, and backoff ran
  0.5 → 1 → 2 → 4 → 8s. No silently-dead socket.

An earlier run appeared to show a connection surviving a restart. That was a
broken test harness — the kill never fired — not the code. Re-tested properly.

## Mutation tested
- Lag notice parsed as a block → 1 test failed.
- `stop()` cancelling without awaiting → 1 test failed.

## Results
backend-api: 119 passed (was 110), ruff clean.

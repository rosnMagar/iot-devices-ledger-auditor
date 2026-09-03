# IOT-29: WebSocket integration test — the Weekend 4 capstone

**Sprint:** sprint-03
**Story points:** 3
**Status:** Review
**Depends on:** IOT-28

## Story
As a developer, I want an end-to-end WebSocket test so that the POST → writer thread → broadcast → WS pipeline is proven.

## Acceptance criteria
- [x] A doctest fixture starts the HTTP server, the WS server, the broadcaster and the writer thread together in one process
- [x] A WebSocket client connects to `/blocks` using the vendored library's client
- [x] After a `POST /events`, the client receives a text frame whose JSON equals the block the POST returned
- [x] **Deterministic — no fixed `sleep` as the synchronisation mechanism.** Frame waits block on a `std::condition_variable` fed by the on-message callback, with a timeout that fails loudly rather than hanging
- [x] A second subscriber connected at the same time also receives the block (fan-out)
- [x] A subscriber that connects *after* the POST receives nothing (late-subscriber isolation)
- [x] The block that arrives over the socket is byte-identical to the one `GET /blocks` returns for that index
- [x] `ctest` green, and green when run repeatedly (`ctest --repeat until-fail:20`)

## Amended 2026-09-01 (during IOT-28)
Two premises in the original ticket no longer hold. Recorded here rather than
silently worked around.

**1. Ephemeral ports are not available for the WS listener.**
`SocketServer::getPort()` (`IXSocketServer.cpp:551`) returns the port it was
*configured* with; it does not resolve an OS-assigned port after binding, the way
httplib's `bind_to_any_port` does. So the WS side cannot ask for port 0 and be
told what it got.

The HTTP side still uses `bind_to_any_port`. The WS side walks up from a base
port until a bind succeeds — the approach already proven in `tests/test_ws.cpp`.
Do not hard-code a port: parallel `ctest` runs and a dev box with storage-core
already running both collide.

**2. No hello frame is needed.**
The original note suggested that if the library offered no "subscribed"
callback, the server should send a hello frame on subscribe so the test could
wait for it. It doesn't need to: `WsServer::connection_count()` exposes exactly
that, so the test waits on the server's own view of the connection instead.

This matters beyond convenience — a hello frame would be **wire protocol added
purely for testability**, and every real client would then have to know to skip
it. The test observing server state directly costs the product nothing.

## Scope note: what this actually adds over IOT-28
`tests/test_ws.cpp` (IOT-28) already covers the WebSocket half thoroughly — 11
cases driving real clients, including lag, fan-out and disconnect handling — but
it drives the `Broadcaster` *by hand*.

What IOT-29 uniquely proves is the **seam**: that a real `POST /events` travels
handler → queue → writer thread → disk → broadcaster → socket, in one process,
and that what the subscriber receives is exactly what the ledger recorded. That
is the whole Weekend 4 pipeline, and nothing tests it today except a manual
smoke run.

## Deviation: a new fixture, not an extension of `TestServer`
The original note said to extend `TestServer` from `tests/test_server.cpp`
rather than write a second fixture. Doing that would give all 13 existing REST
tests a WS listener they do not use — a port allocation and three extra threads
each, on a suite that is currently fast and stable.

Instead `tests/test_pipeline.cpp` gets a fixture that *composes* the pieces
(`AppState`, `WriteQueue`, `Broadcaster`, writer thread, httplib server,
`WsServer`). It is not a duplicate of `TestServer` — it is the superset that only
the capstone needs, and it keeps WS port churn out of the REST suite.

## Implementation notes
- **This is the flakiest test in the project by construction** — two servers,
  a background thread, and a network client, all racing. Two rules keep it sane:
  wait on a condition variable signalled by the callback (never a fixed sleep),
  and give every wait a timeout that fails the test rather than hanging CI
  forever.
- **Confirm the client is subscribed before issuing the POST**, via
  `connection_count()`. Otherwise the block is published into the void and the
  test fails intermittently on fast machines.
- Late-subscriber isolation is the one case that legitimately needs a bounded
  wait for something *not* to happen. Keep it short and assert on the absence,
  and pair it with a positive assertion afterwards so a silently broken feed
  cannot pass it.
- Capstone for the ring-buffer milestone: this is the test that proves the whole
  Weekend 4 pipeline, so it is worth the care.

## Rewritten for C++ (2026-08-29)
Was: `tokio::test` + `tokio-tungstenite` as a dev-dependency. Becomes a doctest
case using the client side of whichever library IOT-45 vendors; "no arbitrary
sleeps" carries over unchanged, and matters more without async's deterministic
scheduling.

## What was built
`tests/test_pipeline.cpp` — 8 cases / 285 assertions. A `Pipeline` fixture
assembles the service the way `main()` does (chain, queue, writer thread,
broadcaster, REST server, WS listener) and tears it down in the same order.

- **capstone** — `POST /events` reaches a subscriber as *the same block*, compared
  whole (`received == posted`), not merely "a block arrived"
- **consistency** — the streamed frame is byte-identical to the `GET /blocks`
  payload for that index, `.dump()` included. That equality is what lets a
  client use one parser for history and live updates, so it is asserted rather
  than assumed
- **durability** — the streamed block is read back off the ledger file directly.
  A block broadcast but never written would pass every in-memory check and
  vanish on restart
- **fan-out** — two subscribers both receive it
- **late-subscriber isolation** — a client connecting after the POST sees
  nothing, then *does* receive the next one, so a simply-broken feed cannot pass
  by delivering nothing at all
- **burst** — 25 events arrive in chain order with each `prev_hash` matching the
  previous `hash`, and `/verify` still valid
- **concurrency** — 4 clients x 5 POSTs: every index delivered exactly once, none
  duplicated, none missing
- **disconnect** — a subscriber leaving does not disturb the write path

## Verification
- `ctest` 8/8 green; `--repeat until-fail:20` on the pipeline suite green
- Clean under `-fsanitize=thread`
- **Mutation-checked.** A capstone that passes for the wrong reason is worse than
  none, so the writer's `publish()` call was disabled and the suite re-run:
  **all 8 cases failed**. They genuinely exercise the seam rather than asserting
  things that were true anyway. `src/writer.cpp` was restored and re-verified
  clean against git afterwards.

## Note on "no sleeps"
Frame arrival is waited on with a `std::condition_variable` fed by the
on-message callback, with a timeout that fails the case rather than hanging CI.

Two places use a bounded poll instead, and neither is the banned pattern of
"sleep and hope": `wait_for_subscribers` polls the server's own
`connection_count()` until it matches, and the late-subscriber case waits a
short bounded interval for something *not* to arrive. Both fail loudly on
timeout; neither proceeds on a fixed delay.

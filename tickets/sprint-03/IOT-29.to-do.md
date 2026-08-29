# IOT-29: WebSocket integration test

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-28

## Story
As a developer, I want an end-to-end WebSocket test so that the POST → writer thread → broadcast → WS pipeline is proven.

## Acceptance criteria
- [ ] A doctest case starts the HTTP server, the WS server and the writer thread on ephemeral ports, in the style of `TestServer` in `tests/test_server.cpp`
- [ ] A WebSocket client connects to `/blocks` (the vendored library's client, or a hand-rolled one if it ships server-only)
- [ ] After a `POST /events`, the client receives a text frame whose JSON equals the block the POST returned
- [ ] **Deterministic — no `sleep` as the synchronisation mechanism.** The test blocks on a `std::condition_variable` or a `std::future` fed by the WS on-message callback, with a generous timeout that fails loudly rather than hanging
- [ ] A second subscriber connected at the same time also receives the block (fan-out)
- [ ] A subscriber that connects *after* the POST receives nothing (late-subscriber isolation)
- [ ] `ctest` green, and green when run repeatedly (`ctest --repeat until-fail:20`)

## Implementation notes
- **This is the flakiest test in the project by construction** — two servers,
  a background thread, and a network client, all racing. Two rules keep it sane:
  wait on a condition variable signalled by the callback (never a fixed sleep),
  and give every wait a timeout that fails the test rather than hanging CI
  forever.
- Connect the WS client and confirm it is subscribed **before** issuing the
  POST. Otherwise the block can be published into the void and the test fails
  intermittently on fast machines. If the library gives no "subscribed"
  callback, have the server send a hello frame on subscribe and wait for it.
- Reuse the `TestServer` pattern from IOT-23 (temp ledger, `bind_to_any_port`,
  stop-and-join in the destructor) and extend it with the WS listener rather
  than writing a second fixture.
- Capstone for the ring-buffer milestone: this is the test that proves the whole
  Weekend 4 pipeline, so it is worth the care.

## Rewritten for C++ (2026-08-29)
Was: `tokio::test` + `tokio-tungstenite` as a dev-dependency. Becomes a doctest
case using the client side of whichever library IOT-45 vendors; "no arbitrary
sleeps" carries over unchanged, and matters more without async's deterministic
scheduling.

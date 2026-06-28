# IOT-29: WS integration test (tokio-tungstenite)

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-28

## Story
As a developer, I want an end-to-end WebSocket test so that the POST→writer→broadcast→WS pipeline is proven.

## Acceptance criteria
- [ ] `tokio::test` spins up the app on an ephemeral port
- [ ] A `tokio-tungstenite` client connects to `/ws/blocks`
- [ ] After a `POST /events`, the client receives the matching block
- [ ] Test is deterministic (no arbitrary sleeps as the sync mechanism)

## Implementation notes
- Add `tokio-tungstenite` as a dev-dependency.
- Capstone test for the ring-buffer milestone.

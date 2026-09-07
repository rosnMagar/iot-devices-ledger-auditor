# IOT-61: backend-api streams blocks to browsers

**Sprint:** sprint-05
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-60

## Story
As an operator, I want the dashboard to receive new blocks live so that readings appear without reloading.

## Acceptance criteria
- [ ] `WS /ws/blocks` endpoint browsers can connect to
- [ ] One upstream subscription fans out to many browser connections
- [ ] Disconnects are cleaned up; a dead client cannot block the others
- [ ] Blocks are enriched with `device_type`/`location_id` from the registry
- [ ] Tests cover fanout, disconnect cleanup, and a slow consumer

## Implementation notes
- WebSocket rather than SSE is a deliberate call: the broadcaster already exists
  end to end, and the dashboard is the reason for it (see ADR 0008).
- Enrichment is the main argument for relaying rather than pointing browsers at
  `:8081` — a raw block carries `actor`, not a device type or location.
- A slow browser must not stall the upstream reader. Bound the per-connection
  queue and drop, the way `Subscription` does in storage-core.

# IOT-61: backend-api streams blocks to browsers

**Sprint:** sprint-05
**Story points:** 3
**Status:** Review
**Depends on:** IOT-60

## Story
As an operator, I want the dashboard to receive new blocks live so that readings appear without reloading.

## Acceptance criteria
- [x] `WS /ws/blocks` endpoint browsers can connect to
- [x] One upstream subscription fans out to many browser connections
- [x] Disconnects are cleaned up; a dead client cannot block the others
- [x] Blocks are enriched with `device_type`/`location_id` from the registry
- [x] Tests cover fanout, disconnect cleanup, and a slow consumer

## Implementation notes
- WebSocket rather than SSE is a deliberate call: the broadcaster already exists
  end to end, and the dashboard is the reason for it (see ADR 0008).
- Enrichment is the main argument for relaying rather than pointing browsers at
  `:8081` — a raw block carries `actor`, not a device type or location.
- A slow browser must not stall the upstream reader. Bound the per-connection
  queue and drop, the way `Subscription` does in storage-core.

## What was built
- `app/hub.py` — `Subscription` (bounded, drop-oldest), `BrowserHub` (fanout),
  `RegistryEnricher` (cached registry lookups).
- `WS /ws/blocks` in `app/main.py`; `/feed/status` gained browser counters.
- 17 hub tests plus 4 endpoint tests.

## Notes for review
- **The upstream reader never awaits a browser.** `Subscription.put` is
  synchronous and drops its oldest frame when full, mirroring storage-core's own
  `Subscription`. Without that, one slow dashboard would stall the feed for
  every other dashboard.
- A slow client is **not disconnected** — it misses frames instead. The newest
  are kept, because a live dashboard wants current state, not an old backlog.
- The endpoint races a send pump against a receive loop. Without the receive
  side, a browser that closes while we are blocked on the queue goes unnoticed
  until the next block — which on a quiet fleet could be a very long time.
- Enrichment failures are swallowed: a dead database degrades the feed rather
  than stopping it.
- Registry misses are cached too, or an unregistered actor posting continuously
  would hit the database on every block.
- Lag notices are forwarded, not swallowed. A dashboard that silently missed
  blocks would show a stale chart that looks live.

## Verified end to end
Real `storage-core` (built from source, for the 8081 listener) plus
`backend-api`, with three concurrent browser clients:
- all three received **identical** frames from one upstream subscription
- `device` was enriched to `cold-store` from the registry even though the block
  reported `reported-elsewhere` — the registry is authoritative
- an unregistered `ghost-device` enriched to `null` rather than erroring
- `browser_subscribers` returned to 0 after all three closed: no leak
- restarting storage-core took `connects` to 2 with the feed still healthy

## Results
backend-api: 136 passed (was 119), ruff clean. Endpoint tests run stable across
three repeats.

# IOT-27: Block broadcaster — ring buffer per subscriber

**Sprint:** sprint-03
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-25

## Story
As a developer, I want new blocks broadcast to subscribers so that the live feed can push updates instead of polling.

## Acceptance criteria
- [ ] `class Broadcaster` in `include/broadcast.hpp`: `subscribe()` returns a subscription handle, `publish(const Block&)` fans out to all live subscribers
- [ ] Each subscription owns a bounded ring buffer (capacity 64) with its own mutex + condition variable
- [ ] The writer thread calls `publish()` after each successful append
- [ ] A slow subscriber drops the **oldest** buffered block rather than blocking the writer, and records that it lagged
- [ ] A late subscriber receives only blocks published after it subscribed
- [ ] Publishing with zero subscribers is a no-op, not an error
- [ ] Unit test covering: fan-out to two subscribers, late-subscriber isolation, and overflow dropping oldest

## Implementation notes
- **The writer thread must never block on a subscriber.** That is the whole
  reason each subscription gets its own bounded buffer with drop-oldest
  semantics: one stalled dashboard tab cannot stop the ledger from accepting
  writes. This is the "ring buffer" the weekend is named after.
- Drop-oldest, not drop-newest — the live feed's value is recency.
- Set a `lagged_` counter when dropping so the consumer (IOT-28) can tell the
  client it missed blocks. This is the C++ stand-in for Tokio's
  `RecvError::Lagged`.
- **Unsubscribe must be safe from the subscriber's thread while the writer is
  publishing.** Hold the subscriber-list lock for the fan-out and have the
  handle deregister itself in its destructor; a `shared_ptr` per subscription
  avoids a use-after-free if a client disconnects mid-publish.
- `publish()` should copy the `Block` into each buffer. Sharing one
  `shared_ptr<const Block>` is the cheaper option if profiling ever says so.

## Rewritten for C++ (2026-08-29)
Was: `broadcast::channel::<Block>(64)` in `main.rs` with `AppState` carrying a
`broadcast::Sender<Block>`. Tokio's `broadcast` is exactly a per-receiver ring
buffer with lag reporting; C++ has no equivalent in the standard library, so this
ticket now includes building the small piece of it we need.

# IOT-27: Block broadcaster — ring buffer per subscriber

**Sprint:** sprint-03
**Story points:** 2
**Status:** Review
**Depends on:** IOT-25

## Story
As a developer, I want new blocks broadcast to subscribers so that the live feed can push updates instead of polling.

## Acceptance criteria
- [x] `class Broadcaster` in `include/broadcast.hpp`: `subscribe()` returns a subscription handle, `publish(const Block&)` fans out to all live subscribers
- [x] Each subscription owns a bounded ring buffer (capacity 64) with its own mutex + condition variable
- [x] The writer thread calls `publish()` after each successful append
- [x] A slow subscriber drops the **oldest** buffered block rather than blocking the writer, and records that it lagged
- [x] A late subscriber receives only blocks published after it subscribed
- [x] Publishing with zero subscribers is a no-op, not an error
- [x] Unit test covering: fan-out to two subscribers, late-subscriber isolation, and overflow dropping oldest

## What was built
- **`Subscription`** — a `std::deque` capped at 64 (configurable), its own mutex
  and condition variable. `next()` blocks; `try_next()` doesn't. On overflow it
  pops the front and increments `lagged_`.
- **`Subscription::Item`** — `{Block block; std::size_t lagged;}`. The lag is
  reported **once**, on the first delivery after the drops, then cleared — the
  C++ stand-in for Tokio's `RecvError::Lagged`, so IOT-28 can tell the client it
  missed blocks rather than showing a feed with silent holes.
- **`Broadcaster`** — `subscribe()`, `publish()`, `close_all()`,
  `subscriber_count()`. Publishing with no subscribers is a plain no-op.
- **`writer_loop` publishes after the append**, once the block is both on disk
  and visible to readers, so a subscriber can never be told about a block that
  `GET /blocks` would not yet return. Skipped entirely when no broadcaster is
  attached.
- `AppState` gained `Broadcaster*`, and `main()` owns one declared *before* the
  writer thread so it outlives it.

## Deviation: weak_ptr registry, not destructor-deregistration
The ticket suggested the handle deregister itself from the Broadcaster in its
destructor. It holds a `vector<weak_ptr<Subscription>>` instead, pruned during
`publish()` and `subscribe()`.

Same observable behaviour — dropping a handle removes the subscriber, and
nothing accumulates — but it avoids two hazards the callback version has: the
destructor would need the subscriber-list lock at exactly the moment `publish()`
may hold it, and a handle outliving its Broadcaster would leave a dangling
back-pointer. The `shared_ptr` the ticket asks for is still what makes a
mid-publish disconnect safe: `publish()` upgrades the weak_ptr and holds the
buffer alive for the duration of the push.

## Testing
`tests/test_broadcast.cpp` — 14 cases / 58 assertions:
- fan-out to two subscribers; draining one leaves the other's buffer untouched
- a late subscriber starts empty and sees only what follows, while the early one
  still has all three in order
- overflow drops the **oldest**: capacity 3, publish 5 → buffer holds a3/a4/a5,
  `lagged == 2` on the first delivery and `0` on the next
- **a slow subscriber never blocks the publisher** — 500 blocks into a capacity-4
  buffer that nothing is draining, publisher completes, `lagged == 496`, while a
  subscriber that kept up loses nothing
- `next()` blocks until a block arrives; `close()` wakes it; a closed
  subscription still delivers what was already buffered
- subscribe/unsubscribe churn (4 threads × 200) against a continuous publisher
- 4 concurrent publishers × 100 blocks, none lost

`tests/test_writer.cpp` — 4 new cases (20 total / 495 assertions):
- the writer publishes exactly the block the caller was handed back (same index
  and hash, not a re-derived one)
- **a block that failed to persist is never published** — publishing something
  not on disk would show subscribers state a restart would erase
- a writer with no broadcaster attached still appends
- 50 appends against a stalled capacity-4 subscriber all complete: the write
  path does not wedge behind a dead dashboard tab

`ctest` 6/6 green; `--repeat until-fail:20` green; `test-broadcast`,
`test-writer` and `test-server` all clean under `-fsanitize=thread`.

## Implementation notes
- **The writer thread must never block on a subscriber.** That is the whole
  reason each subscription gets its own bounded buffer with drop-oldest
  semantics: one stalled dashboard tab cannot stop the ledger from accepting
  writes. This is the "ring buffer" the weekend is named after.
- Drop-oldest, not drop-newest — the live feed's value is recency, and a client
  can backfill from `GET /blocks`.
- `publish()` holds the subscriber-list lock across the fan-out. Safe because
  `push()` never blocks, and a subscriber's `next()` only ever takes its own
  mutex, so there is no lock-order inversion.
- `publish()` copies the `Block` into each buffer. A shared
  `shared_ptr<const Block>` is the cheaper option if profiling ever asks for it.

## Rewritten for C++ (2026-08-29)
Was: `broadcast::channel::<Block>(64)` in `main.rs` with `AppState` carrying a
`broadcast::Sender<Block>`. Tokio's `broadcast` is exactly a per-receiver ring
buffer with lag reporting; C++ has no equivalent in the standard library, so this
ticket now includes building the small piece of it we need.

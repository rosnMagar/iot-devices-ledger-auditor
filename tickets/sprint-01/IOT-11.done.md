# IOT-11: `chain.hpp/.cpp` — `Chain` with `append` / `latest` / `size`

**Sprint:** sprint-01
**Story points:** 3
**Status:** Review
**Depends on:** IOT-10

## Story
As a developer, I want an in-memory chain so that events can be appended as linked blocks.

## Acceptance criteria
- [x] `Chain` wraps `std::vector<Block>`; the constructor seeds a genesis block
- [x] `append(event) -> const Block&` builds the next block from `latest().hash` and pushes it
- [x] `latest() -> const Block&` returns the tail; `size()` returns chain length
- [x] New block's `index` = previous + 1, `prev_hash` = previous `hash`

## Implementation notes
- `append` computes index/prev_hash internally — callers pass only the `EventPayload`.
- Keep `Chain` single-threaded here; concurrency (writer thread/queue) comes in sprint-03.

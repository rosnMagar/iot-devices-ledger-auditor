# IOT-11: `chain.rs` — `new` / `append` / `latest`

**Sprint:** sprint-01
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-10

## Story
As a developer, I want an in-memory chain so that events can be appended as linked blocks.

## Acceptance criteria
- [ ] `Chain` wraps `Vec<Block>`; `Chain::new()` seeds a genesis block
- [ ] `append(event) -> &Block` builds the next block from `latest().hash` and pushes it
- [ ] `latest() -> &Block` returns the tail; `len()` returns chain length
- [ ] New block's `index` = previous + 1, `prev_hash` = previous `hash`

## Implementation notes
- `append` computes index/prev_hash internally — callers pass only the `EventPayload`.
- Keep `Chain` lock-free here; concurrency (mpsc/writer task) comes in sprint-03.

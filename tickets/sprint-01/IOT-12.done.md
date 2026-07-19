# IOT-12: `chain.hpp/.cpp` — `verify()`

**Sprint:** sprint-01
**Story points:** 2
**Status:** Review
**Depends on:** IOT-11

## Story
As an auditor, I want chain verification so that tampering can be detected.

## Acceptance criteria
- [x] `verify()` recomputes each block's hash and checks it matches `block.hash`
- [x] Checks `block[i].prev_hash == block[i-1].hash` for all i
- [x] Returns a small struct identifying validity + first invalid index (if any)
- [x] Genesis block validated against `"0"*64` prev_hash

## Implementation notes
- Return a struct: `{ bool valid; size_t chain_length; size_t checked_blocks; std::optional<size_t> first_invalid_index; }`.
- This logic backs the future `GET /verify` endpoint (sprint-02).

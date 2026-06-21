# IOT-12: `chain.rs` — `verify()`

**Sprint:** sprint-01
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-11

## Story
As an auditor, I want chain verification so that tampering can be detected.

## Acceptance criteria
- [ ] `verify()` recomputes each block's hash and checks it matches `block.hash`
- [ ] Checks `block[i].prev_hash == block[i-1].hash` for all i
- [ ] Returns a result identifying validity + first invalid index (if any)
- [ ] Genesis block validated against `"0"*64` prev_hash

## Implementation notes
- Return a small struct: `{ valid, chain_length, checked_blocks, first_invalid_index }`.
- This logic backs the future `GET /verify` endpoint (sprint-02).

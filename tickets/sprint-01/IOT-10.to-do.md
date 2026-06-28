# IOT-10: `block.rs` — `compute_hash` + `genesis`

**Sprint:** sprint-01
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-9

## Story
As a developer, I want deterministic block hashing so that the chain is tamper-evident.

## Acceptance criteria
- [ ] `compute_hash` = `SHA256(index.to_le_bytes() || timestamp.to_rfc3339() || serde_json::to_string(event) || prev_hash)`, hex-encoded
- [ ] `Block::genesis()` → index 0, `prev_hash = "0"*64`, hash computed
- [ ] `Block::new(index, event, prev_hash)` computes and stores `hash`
- [ ] Same inputs always produce the same hash

## Implementation notes
- Use `sha2::Sha256` + `hex::encode`.
- Hash field ordering must be stable — document it; see `docs/decisions/0001-hash-chain-no-pow.md`.

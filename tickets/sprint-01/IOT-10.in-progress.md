# IOT-10: `block.hpp` — `compute_hash` + `genesis`

**Sprint:** sprint-01
**Story points:** 3
**Status:** In Progress
**Depends on:** IOT-9

## Story
As a developer, I want deterministic block hashing so that the chain is tamper-evident.

## Acceptance criteria
- [x] `compute_hash(index, timestamp, event, prev_hash)` = `SHA256(index as 8 LE bytes || timestamp || json(event).dump() || prev_hash)`, hex-encoded
- [x] `genesis()` → index 0, `prev_hash = "0"*64`, hash computed
- [x] `make_block(index, event, prev_hash)` computes and stores `hash`
- [x] Same inputs always produce the same hash

## Implementation notes
- Use OpenSSL `libcrypto` (`SHA256` / EVP) + a hex encode; link `-lcrypto` in CMake.
- Hash field ordering must be stable — document it; see `docs/decisions/0001-hash-chain-no-pow.md`.

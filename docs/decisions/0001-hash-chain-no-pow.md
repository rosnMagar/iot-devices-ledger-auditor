# 0001 — Hash-chained ledger, no proof-of-work

Status: Accepted

## Context

The ledger needs to be tamper-evident: any edit to a past event should be detectable. There is a single trusted writer (storage-core itself) — there's no need for distributed consensus or Sybil resistance.

## Decision

Each `Block` stores `SHA256(index || timestamp || event_json || prev_hash)`. Blocks link via `prev_hash`, forming a hash chain. No nonce, no mining, no proof-of-work, no consensus protocol.

## Consequences

- Tampering with any block's data or order changes its hash and breaks the chain — detectable via `GET /verify`.
- This is **not** a blockchain in the public/decentralized sense — it's a tamper-evident log under a single trusted writer. Good interview talking point: knowing when PoW/consensus is/isn't needed.
- Anyone with write access to the raw `ledger.log` file and the ability to recompute hashes could still rewrite history undetected — out of scope for this project (would require append-only storage guarantees, e.g. WORM media or a remote log).

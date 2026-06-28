# Weekend 1 — Core Data Model

Status: Not started

## Goal

Build `block.rs` + `chain.rs`: the `Block`/`EventPayload` types, hashing, and an in-memory `Chain` with `append()`/`verify()`. Unit tests for hash determinism, tamper detection, and multi-block integrity. Stretch: CLI demo printing a 5-block chain + verify result.

## Concepts you need (read/skim before starting)

- **Ownership & borrowing** — *The Rust Book*, ch. 4
- **Structs, methods, derive macros** (`#[derive(Debug, Clone, Serialize, Deserialize)]`) — *The Rust Book*, ch. 5
- **Traits** (for things like `PartialEq` on `Block`, and understanding `serde`'s derive traits) — *The Rust Book*, ch. 10
- `sha2` and `hex` crate docs — for `Sha256::digest()` and hex-encoding the output

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD

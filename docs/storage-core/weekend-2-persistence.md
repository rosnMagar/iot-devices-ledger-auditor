# Weekend 2 — Persistence

Status: Not started

## Goal

Build `storage.rs`: NDJSON append/load (`append_block`, `load_chain`, `open_append`). Wire startup reload-or-genesis into `main.rs`. Tests with `tempfile`: write→reload→verify roundtrip, and a "tamper the file on disk → reload → verify fails" demo.

## Concepts you need (read/skim before starting)

- **Error handling**: `Result`, the `?` operator, `thiserror` vs `anyhow` — *The Rust Book*, ch. 9
- **Generic types, traits, lifetimes** as needed for file I/O signatures — *The Rust Book*, ch. 10 (lifetimes section)
- **Iterators** (for line-by-line file parsing) — *The Rust Book*, ch. 13
- `std::fs` / `std::io` docs (`BufReader`, `BufWriter`, `OpenOptions`)
- `tempfile` crate docs (for tests)

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD

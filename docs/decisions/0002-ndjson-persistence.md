# 0002 — NDJSON for ledger persistence

Status: Accepted

## Context

storage-core needs to persist the chain to disk so it survives restarts, and reload it on startup.

## Decision

Persist as newline-delimited JSON (NDJSON) in `data/ledger.log` — one `Block` JSON object per line. Append-only: `append_block()` writes+flushes a line; `load_chain()` reads line-by-line on startup (empty file → seed a genesis block).

## Consequences

- Trivially demo-able (`tail -f ledger.log`, `cat ledger.log | jq`).
- Simple to implement correctly for a Rust beginner — no custom binary format/parsing.
- Less space-efficient than a binary format; not a concern at this project's scale.
- Stretch goal (if ahead of schedule): a binary length-prefixed format behind a feature flag, to demonstrate the tradeoff explicitly.

# IOT-14: `storage.rs` — `append_block` + `open_append`

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-11

## Story
As a developer, I want blocks persisted to an append-only log so that the ledger survives restarts.

## Acceptance criteria
- [ ] `open_append(path)` opens/creates the log file for appending
- [ ] `append_block(file, &block)` writes one JSON object + newline and flushes
- [ ] Parent directory is created if missing
- [ ] Write errors surface as a `Result`, not a panic

## Implementation notes
- NDJSON, one block per line; see `docs/decisions/0002-ndjson-persistence.md`.
- Use `BufWriter` + `OpenOptions::append(true)`; flush after each line for durability.

# IOT-14: `storage.hpp/.cpp` — `append_block` + `open_append`

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-11

## Story
As a developer, I want blocks persisted to an append-only log so that the ledger survives restarts.

## Acceptance criteria
- [ ] `open_append(path) -> std::ofstream` opens/creates the log file for appending
- [ ] `append_block(std::ofstream&, const Block&)` writes one JSON object + newline and flushes
- [ ] Parent directory is created if missing
- [ ] Write errors surface as a thrown `StorageError` (typed exception), never `std::abort`/crash

## Implementation notes
- NDJSON, one block per line; see `docs/decisions/0002-ndjson-persistence.md`.
- Open with `std::ofstream(path, std::ios::app)`; `std::filesystem::create_directories` on the parent.
- Serialize with `nlohmann::json(block).dump()`; call `.flush()` after each line for durability.
- Check stream state after writing (`if (!os) throw StorageError{...}`) rather than letting failures pass silently.

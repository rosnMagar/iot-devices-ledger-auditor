# IOT-15: `storage.hpp/.cpp` — `load_chain`

**Sprint:** sprint-02
**Story points:** 3
**Status:** Done
**Depends on:** IOT-14

## Story
As a developer, I want the chain reloaded from disk on startup so that history is preserved across restarts.

## Acceptance criteria
- [x] `load_chain(path) -> Chain` reads the log line-by-line into a `Chain`
- [x] Empty/missing file → a new `Chain` seeded with genesis
- [x] Malformed line throws a clear error (does not silently skip)
- [x] Returns a `Chain` ready for use by the server's shared state

## Implementation notes
- Read with `std::ifstream` + `std::getline`; parse each line via `nlohmann::json::parse(...).get<Block>()`.
- **Do not re-verify or recompute hashes here** — load the on-disk blocks verbatim so `GET /verify` can still detect tampering. This needs a `Chain::load(std::vector<Block>)` factory that stores blocks as-is (no genesis re-seed, no recompute), distinct from the genesis-seeding default constructor.
- Malformed JSON → catch the parse exception and rethrow as a `StorageError` naming the line number.

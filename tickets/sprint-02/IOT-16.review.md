# IOT-16: Startup reload-or-genesis in `main.cpp`

**Sprint:** sprint-02
**Story points:** 2
**Status:** Review
**Depends on:** IOT-15

## Story
As an operator, I want the server to load existing ledger state on boot so that restarts are seamless.

## Acceptance criteria
- [x] On startup, `main.cpp` calls `load_chain(LEDGER_PATH)`
- [x] Existing file → chain restored; fresh/empty file → genesis seeded **and written to disk**
- [x] Loaded chain length is logged at startup
- [x] Running the binary twice in a row preserves blocks

## Implementation notes
- Keep `main.cpp` thin: config → load → build shared state (`AppState`) → serve.
- Genesis must be persisted (`append_block`) on a fresh boot so the first read is consistent with disk.
- Replaces the current print-and-exit stub `main()` entirely.

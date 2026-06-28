# IOT-16: Startup reload-or-genesis in `main.rs`

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-15

## Story
As an operator, I want the server to load existing ledger state on boot so that restarts are seamless.

## Acceptance criteria
- [ ] On startup, `main.rs` calls `load_chain(LEDGER_PATH)`
- [ ] Existing file → chain restored; fresh file → genesis seeded and written
- [ ] Loaded chain length is logged at startup
- [ ] `cargo run` twice in a row preserves blocks

## Implementation notes
- Keep `main.rs` thin: config → load → build `AppState` → serve.
- Genesis must be persisted so the first read after a fresh boot is consistent.

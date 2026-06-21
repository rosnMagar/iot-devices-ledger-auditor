# IOT-15: `storage.rs` — `load_chain`

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-14

## Story
As a developer, I want the chain reloaded from disk on startup so that history is preserved across restarts.

## Acceptance criteria
- [ ] `load_chain(path)` reads the log line-by-line into a `Chain`
- [ ] Empty/missing file → a new `Chain` seeded with genesis
- [ ] Malformed line returns a clear error (does not silently skip)
- [ ] Returns a `Chain` ready for use by `AppState`

## Implementation notes
- Use `BufReader::lines()`; parse each with `serde_json::from_str::<Block>`.
- Do not re-verify here — `GET /verify` is the explicit integrity check.

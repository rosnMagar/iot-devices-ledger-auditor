# IOT-17: Persistence tests (roundtrip + tamper-on-disk)

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-16

## Story
As a developer, I want persistence tests so that save/reload and on-disk tamper detection are guaranteed.

## Acceptance criteria
- [ ] Test: append N blocks → `load_chain` → identical chain (roundtrip)
- [ ] Test: corrupt a line on disk → reload → `verify()` fails at that index
- [ ] Test: empty file → `load_chain` seeds genesis
- [ ] Tests use `tempfile`, no shared global state

## Implementation notes
- `tempfile::NamedTempFile` or `tempdir` for isolation.
- The tamper test is a strong demo of tamper-evidence — keep it readable.

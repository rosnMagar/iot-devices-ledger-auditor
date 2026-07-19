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
- [ ] Test: empty/missing file → `load_chain` seeds genesis
- [ ] Tests use a unique temp dir per case (no shared global state); `ctest` green

## Implementation notes
- `doctest` (already vendored); isolate with `std::filesystem::temp_directory_path()` + a unique subdir, cleaned up at the end of each case.
- The tamper test (rewrite a byte on disk → reload → `verify()` flags that index) is a strong demo of tamper-evidence — keep it readable.
- Wire the new `test-storage` target into CTest alongside `test-chain`.

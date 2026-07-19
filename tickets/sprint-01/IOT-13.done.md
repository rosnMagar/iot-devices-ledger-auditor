# IOT-13: Unit tests — determinism, tamper, integrity

**Sprint:** sprint-01
**Story points:** 2
**Status:** Review
**Depends on:** IOT-12

## Story
As a developer, I want unit tests on the data model so that the hashing/verification guarantees are locked in.

## Acceptance criteria
- [x] Test: identical inputs → identical hash (determinism)
- [x] Test: mutating any block field changes its hash (tamper detection)
- [x] Test: a 3-block chain verifies; mutating a middle block fails `verify()` at the right index
- [x] `ctest` green

## Implementation notes
- Pure unit tests with `doctest` (vendored), wired into CMake via `ctest`; no I/O yet (persistence tests come in sprint-02).
- Cover the genesis edge case.

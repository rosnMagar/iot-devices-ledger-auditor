# IOT-13: Unit tests — determinism, tamper, integrity

**Sprint:** sprint-01
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-12

## Story
As a developer, I want unit tests on the data model so that the hashing/verification guarantees are locked in.

## Acceptance criteria
- [ ] Test: identical inputs → identical hash (determinism)
- [ ] Test: mutating any block field changes its hash (tamper detection)
- [ ] Test: a 3-block chain verifies; mutating a middle block fails `verify()` at the right index
- [ ] `cargo test` green

## Implementation notes
- Pure unit tests, no I/O yet (persistence tests come in sprint-02).
- Cover the genesis edge case.

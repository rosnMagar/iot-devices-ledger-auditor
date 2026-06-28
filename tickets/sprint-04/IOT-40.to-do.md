# IOT-40: tests — backend filter/sort + frontend component

**Sprint:** sprint-04
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-36, IOT-39

## Story
As a developer, I want tests on the devices feature so that filtering/sorting behavior is locked in.

## Acceptance criteria
- [ ] Backend: tests for each filter (`status`/`location`/`type`), `sort`/`order`, and invalid-param 400s
- [ ] Backend: "active" derivation tested against the configurable window
- [ ] Frontend: component test for the devices table rendering active/inactive + sort toggle
- [ ] All tests run in CI

## Implementation notes
- Backend tests can stub storage-core `GET /blocks` to control `last_seen`.
- Frontend: a lightweight render/interaction test for the table.

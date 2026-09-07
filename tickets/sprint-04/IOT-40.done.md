# IOT-40: tests — backend filter/sort + frontend component

**Sprint:** sprint-04
**Story points:** 2
**Status:** Review
**Depends on:** IOT-36, IOT-39

## Story
As a developer, I want tests on the devices feature so that filtering/sorting behavior is locked in.

## Acceptance criteria
- [x] Backend: tests for each filter (`status`/`location`/`type`), `sort`/`order`, and invalid-param 400s
- [x] Backend: "active" derivation tested against the configurable window
- [x] Frontend: component test for the devices table rendering active/inactive + sort toggle
- [x] All tests run in CI

## Implementation notes
- Backend tests can stub storage-core `GET /blocks` to control `last_seen`.
- Frontend: a lightweight render/interaction test for the table.

## What was built
- Frontend test framework: vitest + jsdom + Testing Library, `npm test`.
- `src/DevicesTable.test.tsx` — 12 tests: status rendering, never-reported,
  unassigned location, both empty states, and the sort controls.
- `src/query.test.ts` — 20 tests: URL round trip, param naming, junk handling,
  sort toggling.
- `src/relativeTime.test.ts` — 12 tests including the timezone regression guard.
- `backend-api/tests/test_activity.py` — two new tests proving the active window
  is really configurable.
- `test_devices_reports_the_configured_window` strengthened.
- CI: frontend job gained a `test` step and moved to `npm ci` + cache.
- `frontend/package-lock.json` committed.

## Notes for review
- **Backend AC 1 was already met** by IOT-36's `test_devices_query.py` (12 tests
  covering each filter, combination, all three sorts, and the 400s). Nothing was
  added there.
- **AC 2 was a real gap.** Every window test pinned 300 via the `window_300`
  fixture, so a hardcoded 300 would have satisfied all of them. Confirmed by
  replacing the setting with a literal 300: the 51 existing tests all passed and
  only the two new ones failed.
- The lockfile is committed so CI resolves the versions the tests were written
  against. `frontend/Dockerfile` still runs `npm install` and does not copy the
  lockfile, so **production image builds remain unpinned** — deliberately left
  for its own ticket, since it changes the deploy path.
- Test files live beside their subjects in `src/`. The production bundle hash is
  unchanged, so they are not shipped.

## Results
- storage-core: 9/9 ctest (unchanged)
- backend-api: 53 passed, ruff clean
- frontend: 44 passed

# IOT-70: auditor docs and end-to-end test

**Sprint:** sprint-06
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-69

## Story
As a developer, I want the auditor documented and covered end to end so that Phase 4 is genuinely finished.

## Acceptance criteria
- [ ] `docs/auditor.md` rewritten from "Not Started" to the real design
- [ ] All three "Open Questions" resolved or removed
- [ ] End-to-end test: seeded anomaly → findings → stubbed LLM → stubbed delivery
- [ ] `auditor` job in CI runs the real suite, not just the stub test
- [ ] Roadmap Phase 4 ticked

## Implementation notes
- `auditor/src/handler.test.ts` currently only asserts that a function is
  exported. That is a placeholder, not coverage.
- The end-to-end test is what proves the pieces connect — each ticket above
  tests its own part in isolation.

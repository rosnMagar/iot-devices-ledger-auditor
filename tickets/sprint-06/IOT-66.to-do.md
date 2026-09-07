# IOT-66: Fetch recent blocks and evaluate the rules

**Sprint:** sprint-06
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-65

## Story
As the auditor, I want to pull the recent ledger and evaluate the rules so that anomalies are found without an LLM in the loop.

## Acceptance criteria
- [ ] Fetches the last 50 blocks via `GET /blocks` (`from = chain_length - 50`)
- [ ] Calls `GET /verify` and treats a failure as an anomaly
- [ ] Each rule from IOT-65 implemented as a pure, separately testable function
- [ ] Returns a structured findings list, empty when nothing is wrong
- [ ] Handles storage-core being unreachable without a false "all clear"
- [ ] Jest tests per rule, including the boundaries

## Implementation notes
- The handler today is a stub that calls `/verify` and logs. This replaces it.
- Detection must be deterministic and testable on its own. The LLM (IOT-67)
  writes prose about findings; it must never be what decides there is a finding.
- `from = chain_length - 50` underflows on a short chain — clamp at 0. The
  off-by-one here is the same shape as the IOT-56 cursor bug.
- Unreachable storage-core reporting "no anomalies" would be the worst possible
  failure mode for a monitoring tool.

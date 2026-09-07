# IOT-69: Schedule the auditor

**Sprint:** sprint-06
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-68

## Story
As an operator, I want the auditor to run on its own so that anomalies are found without me invoking it.

## Acceptance criteria
- [ ] EventBridge schedule wired in `template.yaml`
- [ ] Interval configurable, with the chosen default justified
- [ ] Timeout and memory set deliberately, not left at defaults
- [ ] Deploy documented in `docs/deployment.md`
- [ ] A manual invoke path retained for testing

## Implementation notes
- `docs/auditor.md` lists schedule frequency as open. Frequency trades cost and
  LLM spend against detection latency — state the trade in the ADR.
- Lambda is non-VPC and reaches EC2 over the public IP; the security group must
  allow it. The Phase 0 stub already proved this path works.
- If `EC2_HOST` changes, the Lambda's `STORAGE_CORE_URL` goes stale — the same
  trap as IOT-57. Prefer a single source for the address.

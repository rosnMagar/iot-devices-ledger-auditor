# IOT-65: Define the anomaly detection rules

**Sprint:** sprint-06
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-58

## Story
As a developer, I want the anomaly rules written down so that the auditor has something specific to detect.

## Acceptance criteria
- [ ] Each rule stated with its threshold and the reasoning behind it
- [ ] Rules are expressed per `sensor_type`, not per device (ADR 0010)
- [ ] Chain-integrity failure (`GET /verify`) is treated as its own class of alert
- [ ] ADR `0012-anomaly-detection-rules.md` records rules and rejected options
- [ ] `docs/auditor.md` "Open Questions" updated

## Implementation notes
- `docs/auditor.md` currently lists "exact anomaly-detection rules" as an open
  question. This closes it.
- Candidate rules: reading outside a plausible range, a device going silent past
  its window, a sudden jump between consecutive readings, and a failed hash
  chain verification.
- Thresholds depend on the payload contract from IOT-58, hence the dependency.
- A tamper-evident ledger reporting a broken chain is the highest-severity signal
  the system can produce — it should not be one bullet among many.

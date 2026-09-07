# IOT-68: Deliver the report

**Sprint:** sprint-06
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-67

## Story
As an operator, I want reports delivered somewhere I will see them so that an anomaly is not just a log line.

## Acceptance criteria
- [ ] Report written to S3 with a predictable key layout
- [ ] Notification published to SNS with a link/summary
- [ ] IAM permissions in `template.yaml`, scoped to the bucket and topic
- [ ] Delivery failure is logged loudly, not swallowed
- [ ] Tests with stubbed AWS clients

## Implementation notes
- `docs/auditor.md` lists SNS vs S3 as open — the answer is both: S3 holds the
  full markdown, SNS carries the short notification.
- Bucket must not be public. Reports quote ledger contents.
- Keys by UTC timestamp so they sort chronologically.

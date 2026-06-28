# auditor — Status: Not Started

## Overview (TBD)

Phase 0: stub Lambda that logs and makes one real HTTP call to storage-core's `GET /verify` via the EC2 public IP (validates non-VPC Lambda → public EC2 reachability). Real auditor logic lands in Phase 4.

## Stack / Decisions

- TypeScript, AWS Lambda, deployed via AWS SAM (`template.yaml`)
- No VPC — relies on public EC2 IP + security group
- Trigger: EventBridge schedule or anomaly-triggered invocation (TBD)
- Fetches last 50 blocks via `GET /blocks` (`from = chain_length - 50`)
- Calls an Anthropic LLM to draft a markdown incident report
- Alerts via SNS/S3 (TBD which, or both)

## Open Questions

- Exact anomaly-detection rules (what triggers a report)
- SNS topic / S3 bucket design for report delivery
- Schedule frequency if EventBridge-driven

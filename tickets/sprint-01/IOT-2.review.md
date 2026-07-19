# IOT-2: Provision EC2 `t4g.micro` + security group

**Sprint:** sprint-01
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As an operator, I want a running EC2 host with the right ports open so that the stack can be deployed and reached.

## Acceptance criteria
- [x] `t4g.micro` (Ubuntu 24.04 ARM64, free tier) is running — `52.15.229.12` (us-east-2)
- [x] Key pair created and `.pem` saved securely
- [x] Security group inbound: 22 (my IP only), 80, 8000, 8080
- [x] Public IP recorded in `docs/deployment.md`

## Implementation notes
- Follow `docs/deployment.md` §1.
- Graviton/ARM is required — images are built for `linux/arm64`.
- Tighten 8000/8080 later (Phase 5); open now for the demo + Lambda + ESP32.

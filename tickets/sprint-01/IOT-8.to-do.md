# IOT-8: Deploy auditor Lambda + confirm `/verify` reached

**Sprint:** sprint-01
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-6

## Story
As a developer, I want the auditor Lambda deployed and reaching storage-core so that the non-VPC public-internet path is proven.

## Acceptance criteria
- [ ] `deploy-lambda` job runs `sam build && sam deploy` successfully
- [ ] Lambda exists in AWS and is manually invokable
- [ ] Invocation log shows a successful `GET /verify` to `http://<EC2-IP>:8080`
- [ ] Phase 0 definition-of-done fully checked in `docs/roadmap.md`

## Implementation notes
- `StorageCoreUrl` is passed via `--parameter-overrides` from `EC2_HOST`.
- Test: `aws lambda invoke --function-name iot-ledger-auditor-AuditorFunction out.json`.
- A timeout here usually means the security group is blocking 8080 from the Lambda.

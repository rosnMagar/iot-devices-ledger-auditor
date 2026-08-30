# IOT-4: Create IAM deploy user + access keys

**Sprint:** sprint-01
**Story points:** 1
**Status:** Done
**Depends on:** —

## Story
As an operator, I want a scoped IAM user so that GitHub Actions can deploy the Lambda via SAM.

## Acceptance criteria
- [x] IAM user `iot-ledger-deployer` created
- [x] Policies attached: Lambda, CloudFormation, S3 (scoped tighter in Phase 5)
- [x] Access key ID + secret generated and stored for GitHub secrets

## Implementation notes
- Follow `docs/deployment.md` §5.
- Do not commit keys anywhere; they go straight into GitHub secrets (IOT-5).

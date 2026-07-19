# IOT-5: Set GitHub secrets + `AWS_REGION` var

**Sprint:** sprint-01
**Story points:** 1
**Status:** Review
**Depends on:** IOT-2, IOT-4

## Story
As an operator, I want deploy credentials configured in GitHub so that `deploy.yml` can reach EC2 and AWS.

## Acceptance criteria
- [x] Secrets set: `EC2_HOST`, `EC2_USER`, `EC2_SSH_KEY`, `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`
- [x] Variable set: `AWS_REGION`
- [x] Secret names match those referenced in `deploy.yml`

## Implementation notes
- Follow `docs/deployment.md` §6.
- `EC2_SSH_KEY` is the full contents of the `.pem` file.

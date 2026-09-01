# IOT-6: Merge `dev`→`prod`, confirm build/push + SSH deploy

**Sprint:** sprint-01
**Story points:** 2
**Status:** Done
**Depends on:** IOT-1, IOT-3, IOT-5

## Story
As a developer, I want the first `prod` deploy to run so that the stack is live on EC2.

## Acceptance criteria
- [x] PR `dev`→`prod` merged
- [x] `deploy.yml` builds + pushes `:prod` arm64 images to GHCR
- [x] SSH deploy job runs `git pull && docker compose pull && docker compose up -d` on EC2
- [x] `docker compose ps` on EC2 shows all 3 containers up

## Implementation notes
- If GHCR packages are private, make them public or add `docker login ghcr.io` on EC2 (see `docs/branching-and-cicd.md`).
- Watch both `deploy-ec2` and `build-push` jobs.

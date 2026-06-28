# IOT-1: Confirm all 4 CI jobs green on `dev`

**Sprint:** sprint-01
**Story points:** 1
**Status:** To Do
**Depends on:** —

## Story
As a developer, I want all CI jobs passing on `dev` so that the walking skeleton is verified before any deploy.

## Acceptance criteria
- [ ] `storage-core`, `backend-api`, `frontend`, `auditor` jobs all pass on the latest `dev` commit
- [ ] No QEMU/build errors remain in `deploy.yml` image builds
- [ ] A green run is linked in this ticket before moving on

## Implementation notes
- Watch the Actions tab for the latest `dev` push.
- If a job fails, reproduce locally first (`cargo test`, `pytest`, `npm run build`, `npm test`) before pushing a fix.
- Known prior fixes: `frontend/src/vite-env.d.ts` for `import.meta.env`; `--platform=$BUILDPLATFORM` on builder stages.

# IOT-41: Fix arm64 image builds and stale AWS region defaults

**Sprint:** sprint-02
**Story points:** 1
**Status:** In Review
**Depends on:** —

## Story
As the person deploying this, I want CI to push images the EC2 box can actually execute, and region defaults that match where the infrastructure lives, so that a deploy doesn't silently break or land in the wrong region.

## Acceptance criteria
- [x] `deploy.yml` runs `docker/setup-qemu-action` before Buildx so `platforms: linux/arm64` produces real arm64 images
- [x] `deploy.yml` region fallback is `us-east-2`, not `us-east-1`
- [x] `docs/deployment.md` no longer suggests `us-east-1` anywhere

## Implementation notes
- **The arm64 bug:** all three build steps declare `platforms: linux/arm64`, but without QEMU/binfmt registered on GitHub's x86 runners buildx emits amd64 images anyway. On the t4g.micro (Graviton) instance those die with `exec format error`. Found when the IOT-20 deploy became the first run to actually replace the storage-core image on the box.
- **The region default:** `vars.AWS_REGION` is set to `us-east-2` in repo settings, so nothing was ever mis-deployed — the `us-east-1` fallback was a latent trap for the day that variable goes missing. The docs used `us-east-1` as their example value, which is likely how it spread.
- Not covered here: EC2's `.env` was hand-copied before the `RUST_LOG` → `LOG_LEVEL` rename and probably still carries the stale key. It has to be edited on the box; no repo change fixes it.

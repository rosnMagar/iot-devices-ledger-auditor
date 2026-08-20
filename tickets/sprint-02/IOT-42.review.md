# IOT-42: Fix cross-build producing x86 binaries in arm64 images, add post-deploy health check

**Sprint:** sprint-02
**Story points:** 2
**Status:** In Review
**Depends on:** IOT-41

## Story
As the person deploying this, I want the published images to actually run on the Graviton instance, and a failed deploy to fail loudly, so that a broken release can't sit there crash-looping behind a green workflow.

## Acceptance criteria
- [x] `storage-core/Dockerfile` builder stage runs on the target platform, so the compiled binary is arm64
- [x] `backend-api/Dockerfile` is no longer pinned to the build platform
- [x] `deploy-ec2` polls `/health` after `docker compose up -d` and fails the job if the service never answers

## Implementation notes
- **The bug:** `FROM --platform=$BUILDPLATFORM` pins a stage to the machine doing the build (the x86 runner). For storage-core that meant gcc emitted an x86_64 binary, which then got copied into a genuine arm64 runtime stage — so `docker image inspect` reported `arm64` while the binary inside was x86, and the box gave `exec format error`. backend-api is single-stage, so the whole image was amd64.
- The flag also opted those stages out of the QEMU emulation added in IOT-41, which is why that fix alone changed nothing. Keep the QEMU step — it is required for the corrected Dockerfiles to build at all.
- `frontend/Dockerfile` keeps the flag deliberately: its builder emits static JS (no architecture) and the final nginx stage is native, so the fast path is correct there.
- **Why it went unnoticed:** `docker compose up -d` exits 0 once the container is *created*. With `restart: unless-stopped` a crash-looping container looks identical to a healthy deploy from CI's point of view. The health-check loop closes that gap.
- **Cost:** compiling C++ under QEMU is slow, and storage-core now includes the ~10k-line vendored `httplib.h`. If build-push becomes painfully slow, switch those steps to a native arm64 runner instead of emulation.

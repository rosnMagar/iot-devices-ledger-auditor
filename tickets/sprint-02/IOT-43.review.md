# IOT-43: Static-link the C++ runtime and smoke test the storage-core image in CI

**Sprint:** sprint-02
**Story points:** 2
**Status:** In Review
**Depends on:** IOT-42

## Story
As the person deploying this, I want the published image to actually start, and CI to run the
binary rather than merely build it, so that a third startup failure can't reach the box.

## Acceptance criteria
- [x] `storage-core/Dockerfile` links libstdc++/libgcc statically, so the runtime image's
      libstdc++ version no longer matters
- [x] `build-push` starts the published arm64 image and requires it to serve `/health`
- [x] The smoke step fails the job (and therefore skips `deploy-ec2`) when the image won't run

## Implementation notes
- **The bug:** the builder is `gcc:13`, whose libstdc++ exports `GLIBCXX_3.4.31`/`3.4.32`; the
  `debian:bookworm-slim` runtime ships GCC 12's, which stops at `3.4.30`. The compile and the
  image build both succeed — it only fails when the loader resolves symbols, so the container
  crash-loops with `GLIBCXX_3.4.32 not found`.
- This was present all along, hidden behind the IOT-42 `exec format error`: the x86 binary
  never got far enough to resolve symbols.
- **Fix:** `-DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"`. Costs ~2MB and
  decouples the stages permanently. Rejected `FROM gcc:12` (matches bookworm today, but works
  by coincidence — any base bump silently re-breaks it the same way).
- Safe because nothing else in the image is C++; only C libraries are dynamically linked, so
  there is no cross-boundary ABI concern.
- **Smoke step** runs the just-pushed arm64 image under the QEMU from IOT-41, polls `/health`
  for 60s, and dumps `docker logs` on failure. `main()` takes no arguments and always starts
  the server, so there is no `--version`-style shortcut — actually serving a request is the
  test. Image defaults (`0.0.0.0:8080`) mean no env vars are needed.
- Placed immediately after the storage-core build so the job fails before spending QEMU time
  on backend-api and frontend.

## Verified by hand
Both images built natively on amd64 — the mismatch is architecture-independent, so this
reproduces the box's failure without a Graviton instance.

| Image | Result |
|---|---|
| Pre-fix Dockerfile (control) | exits immediately: `GLIBCXX_3.4.32 not found`, `3.4.31 not found` |
| Post-fix Dockerfile | starts, `GET /health` → `200 {"status":"ok"}` |

`ldd` on the fixed binary lists only `libcrypto.so.3`, `libm.so.6`, `libc.so.6` — no
`libstdc++.so.6`.

## Follow-ups (not this ticket)
- The smoke step covers storage-core only. backend-api and frontend are worth the same
  treatment, but neither has failed this way yet.
- `TAG` is absent from `.env.example` while `docs/deployment.md` requires `TAG=prod` on the
  box, and `${TAG:-dev}` in `docker-compose.yml` degrades silently to dev images when `.env`
  is missing — which is what happens on a rebuilt instance. Own ticket.

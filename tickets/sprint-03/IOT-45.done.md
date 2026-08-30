# IOT-45: Vendor a WebSocket library and prove it cross-builds

**Sprint:** sprint-03
**Story points:** 3
**Status:** Done
**Depends on:** —
**Blocks:** IOT-28

## Story
As a developer, I want a vendored WebSocket library that survives the arm64 Docker build so that `/ws/blocks` can be implemented without surprising the deploy pipeline.

## Acceptance criteria
- [x] A WebSocket library is vendored under `storage-core/third_party/` (nothing via a package manager — see `docs/storage-core/overview.md`)
- [x] `WS_BIND_ADDR` added to `Config` (default `0.0.0.0:8081`) and to `.env.example`
- [x] A trivial echo server on the WS port builds and runs locally
- [x] **The image builds for `linux/arm64` and the binary starts on the box** — confirmed green in CI on the `dev` push, 2026-08-30 — not just compiles
- [x] `docker-compose.yml` publishes 8081; `Dockerfile` gains `EXPOSE 8081`
- [x] EC2 security group note added to `docs/deployment.md` (8081 inbound)
- [x] ADR written recording the library choice and the two-listener consequence

## Library options
Chosen: **IXWebSocket** — BSD-3-Clause (not MIT, as first written), 92 files, no Asio or Boost, plain sockets +
its own thread per connection. Closest in weight to the existing vendored deps.

Fallback: **websocketpp** with **standalone Asio** (`ASIO_STANDALONE`, no Boost).
More established and better documented, but ~650 header files across the two.

Rejected: **uWebSockets** — fastest of the three, but it is a whole HTTP server
and adopting it means rewriting every REST handler and dropping cpp-httplib.
Not worth it to add one endpoint.

## The consequence nobody has costed yet: a second listener
cpp-httplib and any WS library each own their own listening socket, so they
**cannot share port 8080**. `/ws/blocks` therefore is not a path on the existing
server — it is a second server on a second port. That changes things outside
storage-core:

- `docs/architecture.md` shows `GET /ws/blocks` on storage-core's `:8080`. It
  becomes `ws://<host>:8081/blocks`.
- The EC2 security group needs 8081 opened, which per ADR 0007 means another
  port exposed to the public internet with no authentication.
- `backend-api`'s planned WebSocket relay (Phase 2) connects to the new port.
- The frontend's live feed URL is baked in at build time via `VITE_API_BASE_URL`,
  so it needs the port too.

If that cost looks worse than the contract change once you see it written down,
Server-Sent Events over the *existing* httplib server remains the cheaper route —
it needs no library, no second port, and no security-group change. Revisit before
starting, not after.

## Implementation notes
- **Test the arm64 build early.** The last two libraries added to this image cost
  two outages (IOT-42 `exec format error`, IOT-43 `GLIBCXX_3.4.32 not found`).
  Anything with its own threading or TLS is a fresh chance to break the
  `-static-libstdc++` link. Build the image and run it before writing handlers.
- Extend the CI smoke step (`deploy.yml`) to open a WS connection, not just curl
  `/health` — otherwise a broken WS listener ships green exactly like the last
  two bugs did.


## What was done
Vendored at upstream commit `85cd886`, BSD-3-Clause — **not MIT**, as this ticket
originally claimed. 39 `.cpp` and 53 headers into
`storage-core/third_party/ixwebsocket/`, with `VENDORING.md` recording provenance
and the two options we depend on.

Upstream's `CMakeLists.txt` is deliberately unused. The sources are compiled into
our own `ixwebsocket` static target, so `IXWEBSOCKET_USE_TLS` and
`IXWEBSOCKET_USE_ZLIB` stay undefined — that is what keeps the library free of
OpenSSL and zlib. Upstream warnings are suppressed with `-w` so they don't drown
ours.

`WS_BIND_ADDR` (default `0.0.0.0:8081`) parses like `BIND_ADDR` and throws the
same way on a malformed value. `tests/ws_probe.cpp` binds a real echo server and
runs under ctest; IOT-29 replaces it.

ADR 0008 records the choice and the second-listener consequence.

## Verified
- `ctest` **4/4** — the three existing suites plus `ws-probe`.
- Image builds; container starts and serves `/health` and `/verify`.
- **Runtime shared libraries unchanged**: `libcrypto`, `libc`, `libm`. No
  `libstdc++` (the IOT-43 static link still holds) and no zlib. Binary 2.3 MB.

## The arm64 criterion — closed by CI
- [x] **The image builds for `linux/arm64` and the binary starts on the box** — confirmed green in CI on the `dev` push, 2026-08-30

This machine has neither `docker buildx` nor QEMU binfmt, so the build above was
**native amd64 only**. That proves the vendored sources compile, link statically
and run — all architecture-independent — but it does **not** prove arm64.

IXWebSocket is portable C++ with no assembly, so an arch-specific failure was
unlikely — but "unlikely" is also what IOT-42 and IOT-43 were, so it needed
proving rather than assuming.

**The `dev` push built the arm64 image green**, and the smoke step in `deploy.yml`
ran that image under QEMU and got a `/health` response. Cross-compilation, static
link and startup all hold with the vendored library. Box ticked.

## Follow-up moved to IOT-28
Extending the CI smoke step to open a WebSocket connection belongs with IOT-28 —
nothing listens on 8081 until that handler exists, so adding the check now would
fail the build.

# 0008 — Vendored IXWebSocket on a second listener

Status: Accepted (implementation: IOT-45, IOT-28)

## Context

`/ws/blocks` was specified for the live block feed, but **cpp-httplib has no
WebSocket support** — zero references in the vendored header — and nothing was
vendored to provide it. The endpoint had no foundation.

Two shapes were available:

- **Server-Sent Events** over the existing httplib server, via its chunked
  content provider. No new dependency, no second port, no security-group change.
  One-directional server → client, which is all a block feed needs.
- **A vendored WebSocket library**, keeping the documented `/ws/blocks` contract.

## Decision

Vendor **IXWebSocket** (BSD-3-Clause) and run WebSocket on a **second listener**,
`WS_BIND_ADDR`, default `0.0.0.0:8081`.

Sources are copied into `storage-core/third_party/ixwebsocket/` and compiled by
our own CMake target, not upstream's. Neither `IXWEBSOCKET_USE_TLS` nor
`IXWEBSOCKET_USE_ZLIB` is defined, which is what keeps the library free of
OpenSSL and zlib.

Rejected: **uWebSockets** — faster, but it is a whole HTTP server, so adopting it
means rewriting every REST handler and dropping cpp-httplib. Also rejected:
**websocketpp + standalone Asio**, at roughly 650 header files against
IXWebSocket's 92 sources.

## Consequences

- **A second port, not a path.** cpp-httplib and IXWebSocket each own a listening
  socket and cannot share 8080. `/ws/blocks` therefore becomes
  `ws://<host>:8081/blocks`. `docs/architecture.md` still draws it on 8080 and
  needs updating when IOT-28 lands.
- **Another unauthenticated public port.** Per ADR 0007 the security group already
  exposes 8080 to arbitrary IPs; 8081 joins it. Phase 5's reverse-proxy work now
  has two ports to cover, not one.
- **Downstream consumers point at the new port**: backend-api's Phase 2 WebSocket
  relay, and the frontend, whose URL is baked in at build time (IOT-52).
- **No TLS on the WebSocket** — plain `ws://`, consistent with the rest of the
  stack being plain HTTP. Phase 5 covers both together.
- **No permessage-deflate.** Blocks are small JSON documents; compression is not
  worth adding zlib to both the builder and runtime images — the exact class of
  change that caused IOT-42 and IOT-43.
- **Build cost**: 39 additional `.cpp` files compiled per image build, and the
  binary grew to about 2.3 MB. Runtime shared-library dependencies are unchanged
  (`libcrypto`, `libc`, `libm`) — the `-static-libstdc++` fix from IOT-43 still
  holds.

## If this proves wrong

SSE remains available and cheap: it needs no library, no second port and no
security-group change. The switch would cost the `/ws/blocks` contract and a
frontend change, nothing structural.

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

## Why WebSocket at all (added 2026-09-01, IOT-53)

This ADR originally justified the *library* and the *second listener*, and
treated the protocol as given. It wasn't examined, so here is the honest record.

**The choice was inherited, not deliberated.** WebSocket appears in the very
first commit — `34a1559`, the Phase 0 scaffold, 2026-06-19 — already stated as
fact: "storage-core (Rust/axum) — hash-chained append-only ledger, REST +
WebSocket API". Under that stack it cost nothing: `axum::extract::ws` ships with
the framework, so a WebSocket endpoint was one handler and no dependency. A
premise that cheap never needed defending.

**The C++ migration (`a3521a7`) changed the arithmetic and the premise carried
forward unexamined.** cpp-httplib has no WebSocket support, so the same endpoint
now costs a vendored library, a second listening socket, a second public port,
and a second component to shut down cleanly. This ADR then framed its decision
as "keeping the documented `/ws/blocks` contract" — preserving a choice whose
original justification had already stopped applying.

**Server-Sent Events, stated fairly**, because it is the genuinely strong
alternative and the first version of this ADR undersold it:

| | SSE | WebSocket |
|---|---|---|
| Port | the existing `:8080` | a second listener, `:8081` |
| Server library | none — httplib's `set_chunked_content_provider` | 39 vendored `.cpp` files |
| Client | `EventSource`, built into browsers | `WebSocket`, built into browsers |
| Reconnect | automatic, with `Last-Event-ID` resumption | hand-rolled |
| Direction | server → client only | bidirectional |
| Payload | UTF-8 text | text or binary |

SSE's automatic resumption is the underrated part: `Last-Event-ID` maps directly
onto our block index, so a dropped client could resume exactly where it left off
for free. Against WebSocket, that has to be written.

**Ratified anyway, for reasons that are about this project rather than this
protocol:**

- **The cost is already sunk.** As of IOT-45 the library is vendored, compiling
  and CI-green on arm64. Outside `third_party/`, exactly 7 files reference it.
  Switching now would mean *undoing* working code, not avoiding future work.
- **Bidirectional headroom.** A client → server channel already exists if the
  feed ever needs subscription filters or acks. SSE would need a separate POST
  route bolted alongside.
- **It is an explicit learning goal.** `docs/storage-core/weekend-4-ring-buffer-ws.md`
  lists "WebSocket basics — the HTTP Upgrade handshake and text framing" as
  something this weekend is meant to teach. This is a portfolio project; that
  objective is a real input, not a rationalisation.

**What would change the answer:** if the second public port becomes a problem in
Phase 5's reverse-proxy work, or if the per-image build cost of 39 extra
translation units starts to hurt, SSE is the fallback and remains cheap.

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
  `ws://<host>:8081/blocks`. Updated across the docs when IOT-28 landed
  (IOT-53): `architecture.md`, `storage-core/overview.md`, `backend-api.md`,
  `frontend.md` and the roadmap all name the port now.
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

Concretely, reverting means: delete `storage-core/third_party/ixwebsocket/`,
drop `src/ws.cpp` and its two test binaries, revert the seven referencing files
(`CMakeLists.txt`, `Dockerfile`, `include/config.hpp`, `.env.example`,
`docker-compose.yml`, plus this ADR and `docs/deployment.md`), and close 8081.
The broadcaster from IOT-27 is transport-agnostic and would carry over
untouched — it is the piece worth keeping either way.

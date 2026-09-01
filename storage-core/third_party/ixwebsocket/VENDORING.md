# IXWebSocket — vendored

Upstream: https://github.com/machinezone/IXWebSocket
Commit:   85cd886 (shallow clone, 2026-08-30)
License:  BSD-3-Clause (see LICENSE.txt)

## What was copied
Only `ixwebsocket/*.h` and `ixwebsocket/*.cpp` — the library sources. Upstream's
own `CMakeLists.txt` is deliberately **not** used; the sources are compiled
directly into an `ixwebsocket` static target by `storage-core/CMakeLists.txt`, so
the build options below are ours to control rather than theirs.

## Compile-time options we rely on
Neither of these defines is set, which is what keeps the library self-contained:

- **`IXWEBSOCKET_USE_TLS`** — off. No OpenSSL/MbedTLS for the socket layer, so no
  `wss://`. The WebSocket endpoint is plain `ws://`, consistent with the rest of
  the stack being plain HTTP (see ADR 0007). Phase 5's TLS work covers both.
- **`IXWEBSOCKET_USE_ZLIB`** — off. Disables permessage-deflate compression, and
  with it the only hard external dependency. Blocks are small JSON documents;
  compression is not worth adding zlib to the builder *and* runtime images.

If either is ever enabled, the Dockerfile needs the matching `-dev` package in
the builder stage and the shared library in the runtime stage — precisely the
kind of change that caused IOT-42 and IOT-43.

## Updating
Re-copy the same two globs from a new upstream checkout and update the commit
above. Nothing here is patched, so an update is a clean overwrite.

# IOT-20: `server.hpp/.cpp` — httplib server + `AppState` (`shared_mutex`) + `/health`

**Sprint:** sprint-02
**Story points:** 3
**Status:** In Review
**Depends on:** IOT-16, IOT-18

## Story
As an API consumer, I want a running HTTP server with shared state so that endpoints can read/write the chain.

## Acceptance criteria
- [x] `AppState { Chain chain; std::shared_mutex mtx; std::ofstream log; }` — readers take a shared lock, the writer a unique lock
- [x] `httplib::Server` with `GET /health` → `{"status":"ok"}`
- [x] Request logging + permissive CORS headers applied to all responses
- [x] Server binds to `BIND_ADDR` and serves

## Implementation notes
- Vendor **cpp-httplib** (`yhirose/cpp-httplib`) as `storage-core/third_party/httplib.h` (single header, like doctest); link `Threads::Threads` in CMake. **This is a dependency add — confirm before vendoring.**
- Simple version: handlers lock the `shared_mutex` directly. The writer-task refactor is sprint-03 (IOT-24..26).
- `AppState` is built once in `main()` and passed to `serve()` by reference — `std::shared_mutex` is neither copyable nor movable.
- `/health` deliberately takes no lock, so liveness still answers while a write is in flight.
- httplib defaults to `SO_REUSEPORT` on Linux, which would let a second instance bind the same port and split traffic; `set_socket_options` overrides it with plain `SO_REUSEADDR` so a clash fails loudly.
- Request logs via `set_logger`; CORS via `set_post_routing_handler` (or per-response headers).
- See `docs/storage-core/weekend-3-rest-api.md`.

# IOT-20: `server.hpp/.cpp` — httplib server + `AppState` (`shared_mutex`) + `/health`

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-16, IOT-18

## Story
As an API consumer, I want a running HTTP server with shared state so that endpoints can read/write the chain.

## Acceptance criteria
- [ ] `AppState { Chain chain; std::shared_mutex mtx; std::ofstream log; }` — readers take a shared lock, the writer a unique lock
- [ ] `httplib::Server` with `GET /health` → `{"status":"ok"}`
- [ ] Request logging + permissive CORS headers applied to all responses
- [ ] Server binds to `BIND_ADDR` and serves

## Implementation notes
- Vendor **cpp-httplib** (`yhirose/cpp-httplib`) as `storage-core/third_party/httplib.h` (single header, like doctest); link `Threads::Threads` in CMake. **This is a dependency add — confirm before vendoring.**
- Simple version: handlers lock the `shared_mutex` directly. The writer-task refactor is sprint-03 (IOT-24..26).
- Request logs via `set_logger`; CORS via `set_post_routing_handler` (or per-response headers).
- See `docs/storage-core/weekend-3-rest-api.md`.

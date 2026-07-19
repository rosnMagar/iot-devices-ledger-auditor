# IOT-19: Config (env vars) + logging init

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** —

## Story
As an operator, I want configurable bind address, ledger path, and logging so that the service runs in any environment.

## Acceptance criteria
- [ ] `BIND_ADDR` (default `0.0.0.0:8080`) and `LEDGER_PATH` (default `./data/ledger.log`) read from env
- [ ] A log-level env var drives verbosity; startup logs the resolved config
- [ ] Values map to the keys in `.env.example` / `docker-compose.yml`
- [ ] `docker-compose.yml` + `.env.example`: rename the storage-core `RUST_LOG` key to `LOG_LEVEL` (no longer Rust)

## Implementation notes
- Read env in `main.cpp` with `std::getenv`, falling back to defaults.
- Minimal logger to `stderr` (timestamped `LEVEL message` lines) — no heavy dependency; initialize before anything logs.
- Parse `BIND_ADDR` `host:port` into host + int port for `httplib::Server::listen(host, port)`.

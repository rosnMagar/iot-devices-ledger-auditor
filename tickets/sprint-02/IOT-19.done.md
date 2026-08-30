# IOT-19: Config (env vars) + logging init

**Sprint:** sprint-02
**Story points:** 2
**Status:** Done
**Depends on:** —

## Story
As an operator, I want configurable bind address, ledger path, and logging so that the service runs in any environment.

## Acceptance criteria
- [x] `BIND_ADDR` (default `0.0.0.0:8080`) and `LEDGER_PATH` (default `./data/ledger.log`) read from env
- [x] A log-level env var drives verbosity; startup logs the resolved config
- [x] Values map to the keys in `.env.example` / `docker-compose.yml`
- [x] `docker-compose.yml` + `.env.example`: rename the storage-core `RUST_LOG` key to `LOG_LEVEL` (no longer Rust)

## Implementation notes
- `include/config.hpp`: `Config { bind_host, bind_port, ledger_path, log_level }` + `load_config()` reading env with defaults. `BIND_ADDR` is parsed `host:port`; a malformed value throws `std::invalid_argument` so misconfig fails fast at startup (caught in `main`, logged, exit 1 — no core dump).
- `include/log.hpp`: minimal header-only logger — timestamped `ISO8601 LEVEL message` lines to `stderr`, threshold driven by `LOG_LEVEL` (error/warn/info/debug). Initialized before anything logs.
- `main.cpp` logs the resolved config at startup. `bind_host`/`bind_port` are parsed and logged now but only consumed once the server binds in IOT-20.

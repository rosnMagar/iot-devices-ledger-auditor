# IOT-19: Config (env vars) + tracing init

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** —

## Story
As an operator, I want configurable bind address, ledger path, and logging so that the service runs in any environment.

## Acceptance criteria
- [ ] `BIND_ADDR` (default `0.0.0.0:8080`), `LEDGER_PATH` (default `./data/ledger.log`) read from env
- [ ] `RUST_LOG` drives `tracing_subscriber::EnvFilter`
- [ ] Startup logs the resolved config
- [ ] Values map to the keys in `.env.example` / `docker-compose.yml`

## Implementation notes
- Read env in `main.rs`; fall back to defaults with `unwrap_or_else`.
- Initialize tracing before anything logs.

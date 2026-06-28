# backend-api — Status: Not Started

## Overview (TBD)

Phase 0: stub only (`GET /health`, `GET /blocks` proxies storage-core via `httpx`, hardcoded `GET /users`). Real implementation lands in Phase 2.

## Stack / Decisions

- Python, FastAPI
- SQLite via SQLAlchemy for `users`, `locations`, `settings`, `devices` (see [`db-schema.md`](db-schema.md)); documented Postgres migration path
- Proxies storage-core's `/events` and `/blocks`
- WebSocket relay from storage-core's `/ws/blocks` to the frontend
- CORS enabled for the frontend origin (`CORS_ORIGINS` env var)

## Open Questions

- Auth model for users (Phase 2 design)
- Postgres migration trigger/timeline

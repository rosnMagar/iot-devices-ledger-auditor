# IoT Devices Ledger Auditor

An immutable, hash-chained audit ledger for industrial/IoT event monitoring — with a live dashboard and an AI auditor agent that flags anomalies.

## Architecture

- **storage-core** (Rust/axum) — hash-chained append-only ledger, REST + WebSocket API
- **backend-api** (Python/FastAPI) — user/location/settings DB, proxies storage-core, relays live events
- **frontend** (React/Vite) — dashboard with live event feed
- **auditor** (TypeScript/AWS Lambda) — pulls recent blocks, runs anomaly checks, drafts incident reports via LLM
- **firmware** (C++/ESP32, PoC) — physical sensor devices that post events directly to storage-core

See [`docs/architecture.md`](docs/architecture.md) for the full system diagram and [`docs/roadmap.md`](docs/roadmap.md) for the phase-by-phase build plan.

## Quick start

```sh
docker compose up --build
```

Then:
- `curl localhost:8080/health` — storage-core
- `curl localhost:8000/blocks` — backend-api (proxies storage-core)
- visit `localhost` — frontend

## Docs

All architectural decisions, setup runbooks, and per-service notes live in [`docs/`](docs/README.md).

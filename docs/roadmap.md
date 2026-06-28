# Roadmap

Work proceeds phase-by-phase (and within Phase 1, weekend-by-weekend), each requiring explicit approval before starting. This roadmap tracks completion.

## Phase 0 — Monorepo Scaffold + Walking Skeleton (~1.5-2 weekends)

- [ ] `docker compose up --build` works locally (curl `/health`, `/blocks`; visit frontend)
- [ ] `docs/` skeleton exists with all placeholder files
- [ ] Repo default branch is `dev`; `prod` branch exists
- [ ] Push to `dev` triggers CI (all 4 jobs pass) and builds `:dev` images to GHCR (no AWS deploy)
- [ ] Push to `prod` triggers CI + full deploy (GHCR images, EC2 redeploy, Lambda deploy)
- [ ] `http://<EC2-IP>` shows frontend stub displaying blocks/users via backend-api → storage-core
- [ ] Lambda exists in AWS, manually invokable, log shows successful call to `http://<EC2-IP>:8080/verify`

## Phase 1 — C++ storage-core (~3-4 weekends, centerpiece)

- [ ] Weekend 1 — Core data model (`block.hpp`, `chain.hpp`, SHA-256 hashing, unit tests)
- [ ] Weekend 2 — Persistence (NDJSON append/load, reload-on-startup, tamper-detection test)
- [ ] Weekend 3 — REST API (HTTP router, `Chain` behind a `std::shared_mutex`, error handling, integration tests)
- [ ] Weekend 4 — Ring buffer + WebSocket (producer/consumer queue + writer thread, `/ws/blocks`, capstone integration test)

## Phase 1.5 — IoT Firmware PoC (~1 weekend)

- [ ] ESP32 + DHT22 firmware (PlatformIO/C++) posts `SENSOR_READING` events to `POST /events`
- [ ] `docs/firmware.md` and `docs/decisions/0007-iot-poc-direct-to-storage-core.md` filled in

## Phase 2 — backend-api (~2-3 weekends)

- [ ] Real FastAPI app: SQLite via SQLAlchemy for `users`, `locations`, `settings`, `devices`
- [ ] Real proxy of storage-core `/events` and `/blocks`
- [ ] WebSocket relay from storage-core `/ws/blocks` to frontend
- [ ] `docs/backend-api.md` and `docs/db-schema.md` filled in

## Phase 3 — frontend (~2-3 weekends)

- [ ] Real dashboard: live event feed via WebSocket
- [ ] Views for locations/users/settings/devices, auth
- [ ] `docs/frontend.md` filled in

## Phase 4 — auditor Lambda (~1-2 weekends)

- [ ] EventBridge schedule or anomaly-triggered invocation
- [ ] Fetch last 50 blocks, anomaly detection rules
- [ ] Anthropic LLM call drafting markdown incident report, alert via SNS/S3
- [ ] `docs/auditor.md` filled in

## Phase 5 — Polish (~1-2 weekends)

- [ ] Tighten security groups / nginx+TLS reverse proxy (covers ESP32 traffic)
- [ ] Path-filtered CI, OIDC for AWS deploys
- [ ] Branch protection on `prod`
- [ ] README/demo polish for recruiters

# Roadmap

Work proceeds phase-by-phase (and within Phase 1, weekend-by-weekend), each requiring explicit approval before starting. This roadmap tracks completion.

## Phase 0 — Monorepo Scaffold + Walking Skeleton (~1.5-2 weekends)

- [ ] `docker compose up --build` works locally (curl `/health`, `/blocks`; visit frontend)
- [x] `docs/` skeleton exists with all placeholder files
- [ ] Repo default branch; `prod` branch exists — `prod` exists and **is** the default, which is the intended model; this line's wording still says `dev` and needs correcting (IOT-48)
- [x] Push to `dev` triggers CI (all 4 jobs pass) and builds `:dev` images to GHCR (no AWS deploy)
- [x] Push to `prod` triggers CI + full deploy (GHCR images, EC2 redeploy, Lambda deploy)
- [x] `http://<EC2-IP>` shows frontend stub displaying blocks/users via backend-api → storage-core — IOT-7
- [x] Lambda exists in AWS, manually invokable, log shows successful call to `http://<EC2-IP>:8080/verify` — IOT-8

IOT-7 and IOT-8 are closed. The two still unticked are the local
`docker compose up --build` path and the default-branch wording (IOT-48); IOT-1
and IOT-6 remain open in sprint-01 even though IOT-7/IOT-8 depend on them and are
done.

## Phase 1 — C++ storage-core (~3-4 weekends, centerpiece)

- [x] Weekend 1 — Core data model (`block.hpp`, `chain.hpp`, SHA-256 hashing, unit tests) — IOT-9…13
- [x] Weekend 2 — Persistence (NDJSON append/load, reload-on-startup, tamper-detection test) — IOT-14…17
- [x] Weekend 3 — REST API (HTTP router, `Chain` behind a `std::shared_mutex`, error handling, integration tests) — IOT-18…23
- [ ] Weekend 4 — Ring buffer + WebSocket (producer/consumer queue + writer thread, `/ws/blocks`, capstone integration test) — IOT-24…29, IOT-44, IOT-45

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

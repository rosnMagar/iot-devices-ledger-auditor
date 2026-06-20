# 0007 — IoT firmware PoC posts directly to storage-core

Status: Accepted (implementation: Phase 1.5)

## Context

The longer-term vision includes real ESP32 + sensor devices pushing live data into the ledger. The simplest possible path is for firmware to call `POST /events` on storage-core directly over the public internet; the alternative is routing through backend-api.

## Decision

For the Phase 1.5 proof-of-concept, ESP32 firmware (PlatformIO/C++, `WiFi.h` + `HTTPClient.h` + `ArduinoJson`) connects to WiFi, periodically reads a sensor (e.g., DHT22), builds an `EventPayload` JSON with `event_type: "SENSOR_READING"`, and POSTs directly to `http://<EC2-IP>:8080/events` (storage-core), with `location_id`/`actor` from a gitignored `include/secrets.h`.

## Consequences

- Exercises storage-core's real write path (the same `POST /events` used by the rest of the system) with a real external client — good integration validation.
- **Known limitation, deliberately deferred**: plain HTTP to a public IP/port, no device authentication. Acceptable for a demo; explicitly called out as future work for Phase 5 (TLS + device registration via the `devices` table in backend-api, see `docs/db-schema.md`).
- Bypasses backend-api entirely for sensor ingestion — backend-api's role for IoT devices (Phase 2+) becomes device *registration/management*, not data ingestion.
- Requires storage-core's port (8080) to remain open in the EC2 security group for inbound traffic from arbitrary IoT device IPs, which Phase 5 polish should reassess (e.g., a reverse proxy with per-device auth).

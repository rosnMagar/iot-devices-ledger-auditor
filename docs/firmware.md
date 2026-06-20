# firmware — Status: Not Started

## Overview (TBD)

Phase 1.5 proof-of-concept: ESP32 + sensor (e.g., DHT22) posts `SENSOR_READING` events directly to storage-core. See [`decisions/0007-iot-poc-direct-to-storage-core.md`](decisions/0007-iot-poc-direct-to-storage-core.md).

## Stack / Decisions

- PlatformIO + Arduino framework, C++
- `WiFi.h` + `HTTPClient.h` (ESP32 Arduino core), `ArduinoJson`, Adafruit DHT sensor library
- Connects to WiFi, periodically reads sensor, POSTs `EventPayload` JSON to `http://<EC2-IP>:8080/events`
- `location_id`/`actor` and WiFi credentials from a gitignored `include/secrets.h` (with an `include/secrets.h.example` template)
- Known limitation (deferred to Phase 5): plain HTTP, no device auth

## Open Questions

- Which sensor(s) beyond DHT22 (if any)
- Wiring diagram / pin map — to be added once hardware is in hand
- Read interval (every N seconds)

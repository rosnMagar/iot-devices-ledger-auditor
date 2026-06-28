# IOT-30: firmware — PlatformIO scaffold + `secrets.h.example`

**Sprint:** sprint-03
**Story points:** 2
**Status:** To Do
**Depends on:** —

## Story
As a firmware developer, I want a PlatformIO project so that ESP32 code can be built and flashed.

## Acceptance criteria
- [ ] `firmware/platformio.ini` targets an ESP32 board, Arduino framework
- [ ] Dependencies declared: ArduinoJson, Adafruit DHT sensor library
- [ ] `include/secrets.h.example` template (WiFi creds, `location_id`, `actor`, storage-core URL)
- [ ] `include/secrets.h` gitignored; empty `src/main.cpp` compiles

## Implementation notes
- See `docs/firmware.md` and `docs/decisions/0007-iot-poc-direct-to-storage-core.md`.
- Keep real secrets out of git — only the `.example` is committed.

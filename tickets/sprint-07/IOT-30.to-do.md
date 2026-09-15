# IOT-30: firmware — post readings from a XIAO ESP32S3 Sense

**Sprint:** sprint-07
**Story points:** 5
**Status:** To Do
**Depends on:** —

## Story
As a device, I want to read a sensor and post it to storage-core so that real data enters the ledger.

## Acceptance criteria
- [ ] PlatformIO project targeting `seeed_xiao_esp32s3`, PSRAM enabled
- [ ] `secrets.h` (gitignored) with WiFi creds, `location_id`, `actor`, storage-core URL; `.example` committed
- [ ] Connects to WiFi, reads a DHT22, POSTs to `/events` on an interval
- [ ] **One event per sensor** (ADR 0010): `metadata: {sensor_id, sensor_type, value, unit, seq}`
- [ ] A failed read posts `value: null` — never a missing key, never `0`
- [ ] A real post produces a block, visible on the dashboard

## Implementation notes
This is one `platformio.ini` and one `main.cpp`. WiFi, the sensor read and the
POST are paragraphs of the same file, not separate pieces of work.

**Match `backend-api/tools/simulate_readings.py`.** It already emits the exact
payload the dashboard was built and tested against — copy its shape and
everything downstream works unchanged. Getting this wrong is the expensive
mistake: the device posts successfully, the ledger accepts it, and it shows up
nowhere, which looks like a dashboard bug and is not.

Units go in the `unit` field and are explicit — `celsius`, `percent`, never a
bare `temperature`. `actor` is the **device**; `sensor_id` names the sensor.

**PSRAM must be enabled now**, even though only the camera needs it (IOT-78) —
`-DBOARD_HAS_PSRAM` plus the matching `board_build.psram_type`.

**Pick the DHT22 pin after checking the camera's pin map.** On this board the
camera claims most of the header, and a pin taken from a generic ESP32 pinout
collides in a way that presents as a dead sensor.

The device will post readings for sensors nobody registered, so it shows up as
orphan sensors (IOT-73) until rows exist. Expected, not a bug.

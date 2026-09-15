# IOT-78: firmware — camera streams MJPEG and announces itself

**Sprint:** sprint-07
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-30

## Story
As an operator, I want the device's camera live in the dashboard so that I can see what it sees.

## Acceptance criteria
- [ ] `esp_camera_init` succeeds with the XIAO ESP32S3 Sense pin map
- [ ] HTTP endpoint on the device serves `multipart/x-mixed-replace` JPEG frames
- [ ] **`Access-Control-Allow-Origin` set** on that response
- [ ] POSTs `CAMERA_EVENT` / `stream_online` carrying `url` built from the device's **runtime** IP
- [ ] Re-announces after a WiFi reconnect
- [ ] **No frame data is ever posted to `/events`** (ADR 0011)
- [ ] A camera that fails to initialise leaves the readings still posting

## Implementation notes
The harder half. `esp_camera` init plus an HTTP server serving multipart is more
than a script, and it is where the traps are.

**`backend-api/tools/fake_camera.py` is the reference.** `CameraPane` was built
and tested against exactly that response shape, so matching it means the
dashboard needs no change.

Three things that will each cost an evening if missed:

- **PSRAM.** Without it `esp_camera_init` fails at runtime with an error that
  reads like a wiring fault.
- **The CORS header.** The dashboard is a different origin. Without it the
  browser blocks the stream and shows "Stream unreachable" — nothing wrong on
  the device, nothing in its log. Same class of failure as IOT-57.
- **The announced IP must come from `WiFi.localIP()`**, not a constant. A
  compiled-in address breaks the first time DHCP reassigns, and re-announcing on
  reconnect is what makes a moved device self-correcting — the backend keeps the
  most recent announcement.

Start at SVGA or smaller. A high resolution works at boot and then starves the
stream over WiFi.

One viewer is the realistic target on an ESP32. Don't build for more until it
matters. `stream_offline` is not needed — the dashboard's watchdog already
handles a device that dies without saying so.

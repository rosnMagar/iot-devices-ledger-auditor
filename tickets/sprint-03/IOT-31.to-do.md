# IOT-31: firmware — WiFi connect

**Sprint:** sprint-03
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-30

## Story
As a device, I want to connect to WiFi so that it can reach storage-core.

## Acceptance criteria
- [ ] Connects using credentials from `secrets.h`
- [ ] Retries/reconnects on failure with backoff
- [ ] Connection status logged over serial
- [ ] Proceeds to main loop only once connected

## Implementation notes
- `WiFi.h` from the ESP32 Arduino core.
- Block on connect with a timeout; log the assigned IP.

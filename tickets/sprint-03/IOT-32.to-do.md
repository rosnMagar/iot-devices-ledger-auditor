# IOT-32: firmware — DHT22 read

**Sprint:** sprint-03
**Story points:** 2
**Status:** To Do
**Depends on:** IOT-30

## Story
As a device, I want to read temperature and humidity so that sensor data can be reported.

## Acceptance criteria
- [ ] DHT22 initialized on a configured pin
- [ ] Reads temperature + humidity every N seconds
- [ ] NaN/failed reads are detected and skipped (not posted)
- [ ] Values logged over serial

## Implementation notes
- Adafruit DHT sensor library.
- Respect the sensor's minimum read interval (~2s).

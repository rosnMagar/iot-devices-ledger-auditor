# IOT-33: firmware — build `EventPayload` + `POST /events` + docs

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-31, IOT-32, IOT-21

## Story
As a device, I want to post readings to storage-core so that physical sensor data enters the ledger.

## Acceptance criteria
- [ ] Builds `EventPayload` JSON: `event_type: "SENSOR_READING"`, `location_id`/`actor` from secrets, `metadata: {temperature, humidity}`
- [ ] `HTTPClient` POST to `http://<EC2-IP>:8080/events`; logs status code
- [ ] A successful post produces a new block (verify via `GET /blocks`)
- [ ] `docs/firmware.md` updated with wiring diagram + pin map + setup steps

## Implementation notes
- Serialize with ArduinoJson.
- Known PoC limitation (plain HTTP, no auth) is documented, deferred to Phase 5.

# IOT-21: `POST /events` handler

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-20

## Story
As a device or service, I want to submit an event so that it is appended to the ledger as a block.

## Acceptance criteria
- [ ] Body = `EventPayload` JSON; parsed/validated (malformed → 400 via `ApiError`)
- [ ] Appends a block, persists it via `append_block`, returns 201 + full `Block` JSON
- [ ] Write failure → 500 via `ApiError`
- [ ] This is the same endpoint the ESP32 firmware will call (sprint-03)

## Implementation notes
- Acquire a unique (write) lock, `chain.append(payload)`, `storage::append_block(state.log, block)`, release.
- Parse the body with `nlohmann::json::parse(req.body).get<EventPayload>()`; catch parse errors → `ApiError{400,...}`.
- Refactored onto the mpsc writer task in IOT-26 (sprint-03).

# IOT-21: `POST /events` handler

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-20

## Story
As a device or service, I want to submit an event so that it is appended to the ledger as a block.

## Acceptance criteria
- [ ] Body = `EventPayload` JSON; validated by serde extraction
- [ ] Appends a block, persists it via `append_block`, returns 201 + full `Block` JSON
- [ ] Write failure → 500 via `AppError`
- [ ] This is the same endpoint the ESP32 firmware will call (sprint-03)

## Implementation notes
- Simple version: acquire write lock, `chain.append`, `storage::append_block`, release.
- Refactored onto the mpsc writer task in IOT-26.

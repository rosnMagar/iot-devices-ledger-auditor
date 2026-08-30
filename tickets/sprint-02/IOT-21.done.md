# IOT-21: `POST /events` handler

**Sprint:** sprint-02
**Story points:** 3
**Status:** Done
**Depends on:** IOT-20

## Story
As a device or service, I want to submit an event so that it is appended to the ledger as a block.

## Acceptance criteria
- [x] Body = `EventPayload` JSON; parsed/validated (malformed → 400 via `ApiError`)
- [x] Appends a block, persists it via `append_block`, returns 201 + full `Block` JSON
- [x] Write failure → 500 via `ApiError`
- [x] This is the same endpoint the ESP32 firmware will call (sprint-03)

## Implementation notes
- `parse_event()` wraps `nlohmann::json::parse(body).get<EventPayload>()` and turns any
  `nlohmann::json::exception` into a 400. That one catch covers both malformed JSON
  (`parse_error.101`) and a missing key (`out_of_range.403`), because
  `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` uses `at()`.
- **All five fields are required**, `metadata` included — send `"metadata": {}` when there is
  nothing to attach. Kept strict deliberately: the same macro parses lines back off disk in
  `load_chain`, and relaxing it there would let a truncated ledger line load as a valid block
  instead of failing loudly. Firmware sends a fixed shape anyway.
- Only extra validation is a non-empty `event_type`; an empty one parses fine but produces a
  useless ledger entry.
- The append and the disk write are one critical section under `std::unique_lock`. The block
  is copied out of the chain before the lock drops, because `Chain::append` returns a
  reference into its own vector.
- **Rollback on write failure:** if `append_block` throws `StorageError`, the block is already
  in memory but not on disk. The handler rebuilds the chain from `blocks()` minus the last
  entry via `Chain::load`, then throws `ApiError{500, "failed to persist event"}`. Without
  this, later blocks would chain onto a block a restart never sees, and the whole ledger would
  fail verification on reload. The real message is logged; the caller only sees the generic one.

## Verified by hand
- 201 + full block JSON on a valid event; `prev_hash` links to the previous block's `hash`.
- 400 on malformed JSON, on a missing field, and on an empty `event_type`.
- CORS header present on the 201 (post-routing handler from IOT-20 covers it).
- Restart on an existing ledger continues at the next index with no duplicate genesis.

## Follow-ups (not this ticket)
- `install_error_handling` logs every `ApiError` at ERROR, so client 400s read like server
  faults. Worth downgrading 4xx to INFO/WARN — belongs with IOT-18.
- Refactored onto the mpsc writer task in IOT-26 (sprint-03).

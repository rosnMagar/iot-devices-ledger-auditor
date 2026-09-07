# IOT-58: Define the SENSOR_READING payload contract

**Sprint:** sprint-05
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As a developer, I want the sensor reading payload pinned down so that firmware, backend and charts agree on what a reading is.

## Acceptance criteria
- [x] `metadata` fields, types and units written down for `event_type: "SENSOR_READING"`
- [x] A failed sensor read has a defined representation (not a silently missing field)
- [x] ADR `0009-sensor-reading-payload.md` records the choice and the rejected options
- [x] `docs/architecture.md` links to it

## Implementation notes
- Today `metadata` is free-form `nlohmann::json`. `{temperature, humidity}` appears
  only in IOT-33's acceptance criteria — a blocked ticket — and nothing enforces it.
- Decide units explicitly: `celsius` reads unambiguously, `temperature` does not.
- Blocks are immutable, so a payload mistake is permanent in the chain. This is
  the cheapest moment to get it right.
- storage-core deliberately does not validate `metadata` (it is a generic ledger).
  Decide whether validation belongs in backend-api on the way out instead.

## What was decided
`metadata` for `SENSOR_READING` is a flat object: `celsius` (number|null),
`humidity_pct` (number|null), optional `seq` (integer). Units live in the field
name. A failed read is an explicit `null`, never a missing key and never `0`.
Unknown keys are allowed and ignored. storage-core does not validate — it stays
a generic ledger — so consumers validate on the way out.

See [`docs/decisions/0009-sensor-reading-payload.md`](../../docs/decisions/0009-sensor-reading-payload.md).

## Notes for review
- No code in this ticket. The reference parser lands with `GET /readings`
  (IOT-62), which is the first consumer that needs one.
- IOT-33's acceptance criteria were updated to name these fields — it previously
  said `{temperature, humidity}`, which was the only statement of the shape
  anywhere and specified no units.
- The ADR's claim about existing ledger contents was narrowed to what is
  verifiable from the repo: genesis is `GENESIS`/`metadata: {}` and unaffected.
  Prod ledger contents were not inspected.

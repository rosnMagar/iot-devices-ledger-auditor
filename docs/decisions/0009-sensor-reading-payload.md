# 0009 — SENSOR_READING metadata payload

Status: Accepted (implementation: sprint-05, IOT-58)

## Context

`EventPayload.metadata` is free-form JSON (`nlohmann::json`, see `storage-core/include/block.hpp`). storage-core validates `event_type` is non-empty and nothing else — deliberately, because it is a generic ledger and should not know what a temperature is.

That was fine while nothing read `metadata`. Sprint-05 changes it: live charts (IOT-63), a `/readings` time series (IOT-62), and anomaly thresholds (IOT-65) all have to agree on what a reading looks like. Until now the only statement of intent was `metadata: {temperature, humidity}` in the acceptance criteria of IOT-33 — a ticket blocked on hardware, with no doc, no validation and no consumer.

The timing matters. **Blocks are immutable and hash-chained.** A reading written under the wrong shape cannot be migrated, corrected or deleted; it is in the chain permanently, and rewriting it is precisely what the chain exists to detect. Every reading recorded before this decision is one more row that a later consumer has to special-case forever.

## Decision

For `event_type: "SENSOR_READING"`, `metadata` is a flat JSON object:

| Field | Type | Unit | Required | Notes |
| --- | --- | --- | --- | --- |
| `celsius` | number \| null | °C | yes | `null` = the sensor was read and failed |
| `humidity_pct` | number \| null | % relative humidity | yes | `null` = as above |
| `seq` | integer | — | no | Monotonic per device; detects gaps |

Rules:

1. **Units are in the field name.** `celsius`, not `temperature`. A bare `temperature` is unreadable without out-of-band knowledge of whether a firmware build used °C or °F, and that knowledge is exactly what goes missing.
2. **A failed read is an explicit `null`, not a missing key.** These are different facts: "the device reported and the sensor failed" versus "this block predates the field". Omission cannot distinguish them, and the chain keeps both forever.
3. **`null` is not `0`.** 0 °C is a real temperature. A sentinel value would be indistinguishable from a freezing warehouse.
4. **Unknown extra keys are allowed and ignored** by consumers. Firmware may add fields (battery, RSSI) without invalidating every existing block.
5. **storage-core still does not validate this.** It stays a generic ledger. Validation belongs in backend-api on the way *out*, which is the only place that can afford to be lenient about history.

## Consequences

- Consumers must treat every reading as possibly `null` on both fields. `/readings` (IOT-62) skips them rather than charting a gap as zero; the chart (IOT-63) breaks the line.
- **Existing ledgers predate this contract.** The genesis block carries `event_type: "GENESIS"` and `metadata: {}` (`storage-core/include/block.hpp`), so it is unaffected; but any `SENSOR_READING` written before this ADR was hand-constructed and conforms to nothing. Consumers must therefore tolerate a `SENSOR_READING` whose metadata is absent, empty or unparseable, and skip it rather than erroring.
- The simulator (IOT-59) and the eventual firmware (IOT-33) both target this shape, so switching from simulated to real devices needs no consumer change. IOT-33's acceptance criteria should be updated to name these fields.
- `seq` is optional because the ESP32 loses it across a reboot. Where present it distinguishes "device was offline" from "blocks were dropped"; where absent, consumers fall back to timestamps.
- Adding a *required* field later is a breaking change for every consumer that must then handle its absence in old blocks. Adding optional fields is free. This asymmetry is the reason for keeping the required set as small as it is.

## Rejected alternatives

- **`temperature` with a separate `unit` field.** Every consumer must convert, and a wrong `unit` is unfixable in an immutable block.
- **A nested `readings: {...}` object.** Extra depth for no benefit; a flat object is easier to query and to eyeball in NDJSON.
- **Validating in storage-core.** Would make the ledger domain-aware, and would reject readings during a firmware schema change — losing data precisely when something is already going wrong.
- **Omitting the field on a failed read.** Cheapest for firmware, permanently ambiguous for readers. See rule 2.

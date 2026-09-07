# 0010 — Devices have sensors; readings are per-sensor

Status: Accepted (implementation: sprint-05)
Supersedes: [0009](0009-sensor-reading-payload.md)

## Context

ADR 0009 assumed one implicit sensor set per device: `metadata` carried
`celsius` and `humidity_pct` side by side, because a DHT22 reports both and
nothing else existed.

That assumption does not survive the actual requirement. A device is a board;
sensors are the things attached to it, a device may carry several, and they are
not alike:

- they report at **different rates** — a temperature probe every 30s, a camera continuously;
- they have **different shapes** — a scalar, a 3-axis vector, a video stream;
- they **fail independently** — a dead humidity sensor says nothing about the thermometer beside it;
- they are **added and removed** over a board's life.

A flat per-device payload cannot express any of that. `{celsius, humidity_pct}`
has no room for two thermometers on one board, no way to say which one failed,
and nowhere to put a vector or a stream.

Nothing has been lost by finding this late: ADR 0009 shipped a contract and a
simulator, and the only readings ever written under it were in a throwaway test
container. No production ledger contains them.

## Decision

### Sensors are a registry table

`sensors` joins to `devices`, alongside the existing registry (`docs/db-schema.md`).
A sensor has a `sensor_id`, its owning `device_id`, a `sensor_type`, and a `unit`.
As with `devices`, this records what is *registered* — never what was measured.

### One event per sensor reading

`metadata` for `event_type: "SENSOR_READING"` becomes:

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `sensor_id` | string | yes | Unique within the device |
| `sensor_type` | string | yes | `temperature`, `humidity`, `pressure`, `accelerometer`, `camera`, … |
| `value` | number \| array \| null | yes | `null` = read attempted and failed |
| `unit` | string | yes | `celsius`, `percent`, `hpa`, `m_s2`, … |
| `seq` | integer | no | Monotonic per sensor |

`actor` stays the **device id**, not the sensor. That keeps IOT-35's `last_seen`
derivation working unchanged — activity is a property of the board that reported.

Rules carried forward from 0009, unchanged and for the same reasons:

1. **Units are explicit** — now in a `unit` field rather than the value's name, because `value` is generic.
2. **A failed read is `null`, never a missing key, never `0`.** 0 °C is a real temperature.
3. **Unknown keys are allowed and ignored.**
4. **storage-core still does not validate.** It stays a generic ledger; consumers validate on the way out.

### Vector readings

An accelerometer sends `value: [x, y, z]` with `unit: "m_s2"`. Consumers that
cannot render a vector skip it rather than coercing it — the same rule already
applied to unparseable metadata.

### Camera sensors never carry media

A `camera` sensor is registered like any other and appears in the ledger, but a
block **never contains a frame**. Video reaches the browser out of band; the
ledger records events *about* the camera — stream up/down, motion detected, and
optionally a snapshot's content hash plus its object-store key.

This is not a size optimisation. The chain is append-only and undeletable by
design, which is the product's entire value. Footage of people in a store that
cannot delete is a privacy problem with no remedy after the fact: no retention
policy, no redaction, no erasure request. Keeping media out and hashes in
preserves tamper-evidence over the footage while leaving the footage itself
deletable. See ADR 0011.

## Consequences

- One reading per event means **more blocks**: a 3-sensor board at 30s produces 3× the events. Acceptable — blocks are small and the chain is append-only anyway — but `GET /readings` must filter by `sensor_id` server-side rather than shipping everything to the browser.
- A device with no registered sensors is legitimate (a board awaiting commissioning) and must render as such, not as an error.
- Readings can arrive for a `sensor_id` that is not registered — the same orphan-actor case IOT-35 already handles for devices, and the same cause: a typo in the firmware config.
- The frontend renders **per sensor type**, not per device: a radial gauge for temperature and pressure, a line chart for time series, a video pane for cameras, and later a 3-axis visualiser for accelerometers. A sensor type with no renderer must degrade to showing the raw latest value rather than an empty box.
- `unit` travels with every reading, so a device swapping a °C probe for a °F one is visible in the data rather than silently corrupting a chart.

## Rejected alternatives

- **Keep the flat per-device payload and add fields per sensor type.** Every new sensor widens a payload that every consumer must then handle, and two sensors of the same type on one board remain unrepresentable.
- **One event per device carrying a map of sensor readings.** Compact, but it forces every sensor onto the slowest sensor's clock, and one failed sensor muddies a block that is otherwise fine.
- **A separate `readings` table in SQLite.** Contradicts the rule that the registry never records what happened; the ledger is the source of truth for measurements.
- **Video frames or base64 stills in `metadata`.** Rejected on privacy grounds before size ones — see above.

# IOT-74: sensor dashboard panel

**Sprint:** sprint-05
**Story points:** 5
**Status:** Review
**Depends on:** IOT-62, IOT-73

## Story
As an operator, I want a panel beside the devices table showing a selected device's sensor readings so that I can see its state at a glance.

## Acceptance criteria
- [x] Selecting a device in the table opens a panel on the right
- [x] One visualisation per sensor, chosen by `sensor_type`
- [x] Radial gauge for temperature and pressure; line chart for other series
- [x] An unknown `sensor_type` degrades to the latest raw value, not an empty box
- [x] A sensor with no readings, and a failed (`null`) reading, are visibly distinct
- [x] Layout holds up when a device has one sensor and when it has many
- [x] Component tests per renderer

## Implementation notes
- **Needs a charting dependency** — requires explicit approval. `uplot` is small
  and fast, which matters when several panels redraw on every reading; `recharts`
  is friendlier but much heavier.
- The renderer is chosen by sensor type, so adding a type later means adding a
  renderer, not editing the panel. Accelerometer (3-axis) and others land that way.
- A gauge needs a range. Take it from the sensor's `unit` plus a per-type default,
  and say in the ticket where those defaults live.
- `null` readings break the line rather than plotting zero (ADR 0010).

## What was built
- `sensors.ts` / `useSensorPanel.ts` — types and the two-request load per device.
- `chartData.ts` — pure reading-to-chart shaping, including vectors and nulls.
- `gaugeRange.ts` / `Gauge.tsx` — dial ranges and a plain-SVG radial gauge.
- `SeriesChart.tsx` — uPlot line chart.
- `SensorPanel.tsx` — renderer chosen by sensor type.
- `DevicesTable` — row selection plus a Sensors count column.
- `App.tsx` — table left, panel right, each taking half the width.
- `index.css` — stacks the two columns below 1100px.

## Notes for review
- **uPlot, not recharts.** The bundle went 196.31 kB to 210.48 kB — about 7 kB
  gzipped. recharts is roughly an order of magnitude heavier, and several panels
  redraw on every reading.
- **The gauge is plain SVG.** A single needle does not need a charting library;
  uPlot is for series.
- The gauge range comes from the **unit** first, then the sensor type
  (`gaugeRange.ts`), so a probe swapped from °C to °F is not silently drawn on a
  celsius scale. An out-of-range value pins at the end rather than disappearing,
  because it is still a real measurement.
- Renderer is chosen by `sensor_type`, so IOT-75's camera pane and IOT-76's
  accelerometer 3D view are additions rather than edits to the panel.
- **The panel does not live-update yet.** It loads on selection. IOT-60/61 build
  the stream and IOT-64 swaps polling for it.
- A vector null nulls **every** axis, otherwise the gap shows on x only and
  y/z look like they kept reporting.

## Three states a naive panel would collapse
- `latest: null` — never reported → "No readings yet."
- `latest.value: null` — reported and the read failed → a dash and "read failed",
  never a zero that looks like a measurement.
- Unknown `sensor_type` → the raw latest value, never an empty box.

## Verified in a browser against real containers
Table, backend and ledger all real, driven by the simulator:
- `esp32-03` rendered **two separate gauges** for `temp-0` (22.37°C) and
  `temp-1` (20.09°C) — two thermometers on one board, the capability the
  superseded flat payload could not express
- the accelerometer drew a 3-axis chart with z at ~9.8 (gravity) and a **visible
  break in the line**; `GET /readings` confirmed 3 genuine failed reads at
  seq 1, 7 and 8, so the gap is a real null and not sparse data
- `esp32-02` showed a pressure gauge at 1013.12 hPa, a cold-store temperature of
  4.62°C, and `humi-0` as "— read failed" from a real simulator null
- selecting a row highlighted it; the hint "Select a device to see its sensors"
  showed before any selection

## Results
frontend: 78 passed (was 44), eslint and build clean.

## Layout revision (after review)
The first version gave the panel a 340px minimum beside a 1400px page, which was
too small to read. Changed to:
- the page is full width, no `maxWidth`;
- a CSS grid of `minmax(0, 1fr) minmax(0, 1fr)` when a device is selected, so
  the panel owns half the screen, and a single column when nothing is;
- `SeriesChart` measures its container with a `ResizeObserver` and sizes uPlot to
  it, because uPlot needs pixel dimensions and would otherwise keep whatever
  width it was born with — a wider panel would have bought nothing;
- the gauge grew from 120px to 168px;
- below 1100px the columns stack, since the panel would otherwise be narrower
  than its own charts.

`ResizeObserver` is absent in jsdom, so the hook falls back to a fixed width
rather than throwing.

**Not verified**: the below-1100px stacking. The browser tooling here would not
change the render viewport, so only the built CSS was confirmed to contain the
rule. Worth an eyeball on a narrow window.

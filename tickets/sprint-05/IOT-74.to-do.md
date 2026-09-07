# IOT-74: sensor dashboard panel

**Sprint:** sprint-05
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-62, IOT-73

## Story
As an operator, I want a panel beside the devices table showing a selected device's sensor readings so that I can see its state at a glance.

## Acceptance criteria
- [ ] Selecting a device in the table opens a panel on the right
- [ ] One visualisation per sensor, chosen by `sensor_type`
- [ ] Radial gauge for temperature and pressure; line chart for other series
- [ ] An unknown `sensor_type` degrades to the latest raw value, not an empty box
- [ ] A sensor with no readings, and a failed (`null`) reading, are visibly distinct
- [ ] Layout holds up when a device has one sensor and when it has many
- [ ] Component tests per renderer

## Implementation notes
- **Needs a charting dependency** — requires explicit approval. `uplot` is small
  and fast, which matters when several panels redraw on every reading; `recharts`
  is friendlier but much heavier.
- The renderer is chosen by sensor type, so adding a type later means adding a
  renderer, not editing the panel. Accelerometer (3-axis) and others land that way.
- A gauge needs a range. Take it from the sensor's `unit` plus a per-type default,
  and say in the ticket where those defaults live.
- `null` readings break the line rather than plotting zero (ADR 0010).

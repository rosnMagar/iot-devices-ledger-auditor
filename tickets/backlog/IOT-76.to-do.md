# IOT-76: accelerometer 3-axis visualiser

**Sprint:** backlog
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-74

## Story
As an operator, I want a 3D view of an accelerometer's readings so that orientation and movement are legible at a glance.

## Acceptance criteria
- [ ] `accelerometer` sensors render a 3-axis visualiser
- [ ] Live orientation plus a short history of magnitude
- [ ] Degrades to three line charts where 3D is unavailable
- [ ] Vector `null` (failed read) handled without breaking the view

## Implementation notes
- Deliberately parked. ADR 0010 already defines the payload
  (`value: [x, y, z]`, `unit: "m_s2"`) and IOT-72 emits it, so the data will
  exist well before this is built — nothing here blocks it later.
- Needs a 3D dependency (three.js or similar), which is a heavier ask than the
  charting library in IOT-74. Worth confirming the 2D panel earns its keep first.
- IOT-74's renderer-per-sensor-type design is what makes this additive.

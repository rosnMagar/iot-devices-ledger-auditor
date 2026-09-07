# IOT-63: Live sensor chart

**Sprint:** sprint-05
**Story points:** 5
**Status:** Superseded by IOT-74
**Depends on:** IOT-61, IOT-62

## Story
As an operator, I want a live-updating graph of a device's readings so that I can see trends as they happen.

## Acceptance criteria
- [ ] Chart renders history from `GET /readings` on mount
- [ ] New readings from the WebSocket append without a refetch
- [ ] Axis units come from the payload contract, and are labelled
- [ ] Old points are trimmed so the chart does not grow unbounded
- [ ] Loading, empty and disconnected states are visible
- [ ] Component tests for render, append, and trim

## Implementation notes
- **Needs a charting dependency** — installing it requires explicit approval.
  Candidates: `recharts` (React-native API, heavier), `chart.js` +
  `react-chartjs-2`, or `uplot` (tiny, fastest, more manual).
- The join of backfill and live tail is the bug-prone part: a reading can arrive
  over the socket that the backfill also returned. De-duplicate on block index.
- A disconnected socket must be visible. A frozen chart that looks live is worse
  than one that says it is stale — same reasoning as `ledger_reachable`.

## Superseded
Folded into IOT-74. This ticket assumed one chart per device; ADR 0010 makes a
device a collection of independent sensors, so the unit of rendering is a sensor,
not a device. The live-append and de-duplication notes below still apply and
carry over to IOT-74.

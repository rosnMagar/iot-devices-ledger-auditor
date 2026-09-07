# IOT-75: live camera broadcast in the side panel

**Sprint:** sprint-05
**Story points:** 5
**Priority:** main camera feature — recording/retention (IOT-77) is deferred and optional
**Status:** To Do
**Depends on:** IOT-74

## Story
As an operator, I want a camera sensor's live view in the panel so that I can see what a device sees.

## Acceptance criteria
- [ ] A `camera` sensor renders a live view, not a chart
- [ ] Stream is fetched directly from the device/media source, not through the ledger
- [ ] Stream-down is visibly distinct from ledger-down
- [ ] Camera *events* (`stream_online`, `motion_detected`) still show in the panel
- [ ] No frame data is ever read from or written to a block
- [ ] A simulated camera source exists for development
- [ ] Live only: nothing is recorded or stored anywhere

## Implementation notes
- ADR 0011. MJPEG over HTTP is enough for the PoC; WebRTC/HLS only if latency or
  scale demands it.
- **The stream is a second network path to the device** with its own exposure.
  It is as unauthenticated as the rest of the PoC until Phase 5, so it must not
  point at anything real.
- **Live only.** No recording, no snapshots, no object storage. Nothing is
  persisted, which removes retention, expiry and erasure from the problem
  entirely. IOT-77 covers recording if it is ever wanted.
- The panel keys off **sensor type**, not device type, so it covers both "this
  device is a camera" (its only sensor) and a board carrying a camera alongside a
  thermometer — which is the common ESP32-CAM case.
- A frozen last frame must not look live. Show the stream state explicitly.

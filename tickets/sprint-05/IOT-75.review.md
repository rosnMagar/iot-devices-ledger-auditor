# IOT-75: live camera broadcast in the side panel

**Sprint:** sprint-05
**Story points:** 5
**Priority:** main camera feature — recording/retention (IOT-77) is deferred and optional
**Status:** Review
**Depends on:** IOT-74

## Story
As an operator, I want a camera sensor's live view in the panel so that I can see what a device sees.

## Acceptance criteria
- [x] A `camera` sensor renders a live view, not a chart
- [x] Stream is fetched directly from the device/media source, not through the ledger
- [x] Stream-down is visibly distinct from ledger-down
- [x] Camera *events* (`stream_online`, `motion_detected`) still show in the panel
- [x] No frame data is ever read from or written to a block
- [x] A simulated camera source exists for development
- [x] Live only: nothing is recorded or stored anywhere

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

## What was built
- `app/activity.py` — camera events tracked separately from readings, keeping
  the last announced stream URL.
- `GET /devices/{id}/sensors` — camera sensors carry a `camera` block.
- `tools/fake_camera.py` — a dependency-free multipart stream source.
- Simulator announces `url` on `stream_online`.
- `frontend/src/CameraPane.tsx` + 11 tests.

## Where the stream URL comes from, and why
`sensors` has no column for it, and adding one is not safe: `create_all` never
alters an existing table, so on prod — where `sensors` already exists — a new
column would silently not appear. Rather than contort the schema around that
tooling gap, **the device announces its stream URL in its `stream_online`
event**, which is exactly what ADR 0011 says the ledger is for.

No migration needed, the device is the authority on its own address, and a
camera that moves corrects itself on its next announcement. The tradeoff: that
address is permanent in the chain. If it ever needs to be private or mutable, it
belongs in the registry, and that is the point at which Alembic is required.

## The failure the ticket predicted, found and fixed
The ticket said "A frozen last frame must not look live." It did exactly that.
Killing the camera mid-stream left the last frame on screen, still captioned
"● streaming" — `onError` never fires because the image had already loaded and
the stream merely stopped.

Two fixes: a `stream_offline` event is believed immediately, and a **30s
watchdog remounts the image**, so a dead source is discovered within 30s instead
of never. Residual limitation, stated rather than hidden: between the death and
the next watchdog tick the frame is stale, and a device that dies without
announcing `stream_offline` relies entirely on that watchdog.

## Verified in a browser
Real storage-core, backend-api, the simulator and the fake camera:
- the camera pane rendered live video, and the moving bar visibly moved between
  screenshots — a real stream, not a still
- the caption read "streaming from http://localhost:8090/stream · not recorded"
- killing the camera then reselecting showed "Stream unreachable ... The ledger
  feed is unaffected." **while the ledger indicator still read "● live"** —
  the two outages are visibly distinct, which is the point of that criterion

## Results
backend-api: 146 passed (was 140). frontend: 110 passed (was 98). All clean.

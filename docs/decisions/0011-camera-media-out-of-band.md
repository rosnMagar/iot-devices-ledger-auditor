# 0011 — Camera media never enters the ledger

Status: Accepted (implementation: sprint-05)

## Context

A device may carry a camera, and the dashboard should show its live output
alongside the other sensor panels. The obvious-looking route is the one every
other sensor takes: the device posts to `POST /events` and the frontend reads it
back off the ledger.

For video that route is wrong, and wrong in a way that cannot be corrected after
the fact.

## Decision

**No frame, still, or clip is ever written into a block.** A `camera` sensor is
registered like any other and produces ledger *events* — `stream_online`,
`stream_offline`, `motion_detected`, `snapshot_taken` — but the media itself
travels out of band.

- **Live view**: the browser opens the stream directly (MJPEG over HTTP for the PoC; WebRTC or HLS if latency and scale demand it later). The stream does not pass through storage-core.
- **Stills worth keeping**: written to object storage; the block records the object key and the SHA-256 of the bytes.
- **Recorded video**: deferred and optional (IOT-77). The current scope is **live only — nothing is recorded or stored**. If recording is added later it is stored compressed and retained for 24 hours, then deleted automatically.
- **Tamper-evidence is preserved** by the hash: anyone can re-hash the stored object and compare it against the immutable chain. The chain proves the media has not changed since it was recorded, without ever holding the media.

### Retention (only if recording is ever added)

The current scope stores **no video at all**. Live frames reach the browser and
are gone. That is the strongest available privacy position — there is nothing to
retain, expire, leak, or be asked to erase — and it is the default until someone
asks for playback.

Should recording arrive, video is kept for **24 hours**, compressed, and expires automatically. Expiry is
enforced by the storage layer — an object-store lifecycle rule, not application
code and not a cron job someone has to remember. A retention policy that depends
on a job succeeding is a retention policy that quietly fails open, and the
failure mode is a year of footage nobody meant to keep.

Retention is only expressible because the media is out of the chain. A 24-hour
TTL on an append-only, tamper-evident log is a contradiction: the whole point is
that nothing in it can be removed. Keeping media outside is what makes deleting
it possible at all.

## Consequences

- The footage stays deletable — retention policies, redaction and erasure requests all remain possible. A hash of a deleted object is a harmless orphan; the block stays valid.
- **The hash outlives the media by design.** After 24 hours a block still names an object that no longer exists. Consumers must treat "expired" as a normal, expected state and show it distinctly from "missing when it should be there" — conflating the two makes routine expiry look like tampering, and would eventually make real tampering look routine.
- **Retention would interact with the auditor** (only once recording exists). The auditor (sprint-06) runs on a schedule; if it runs daily and video lives 24 hours, an incident detected late in the window may reference footage that is already gone or about to be. Either the auditor runs often enough to beat expiry, or media relevant to a finding is deliberately promoted out of the expiring bucket before it lapses. This is a real constraint on IOT-69's schedule, not a detail.
- Chain size stays bounded and `GET /verify` stays fast. The chain is re-hashed end to end, so media in the chain would make verification cost grow with footage volume.
- A camera panel can be live while the ledger is unreachable, and vice versa. Both states must be shown distinctly rather than being conflated into one "broken" state.
- The camera stream is a **second network path to the device**, with its own exposure. Phase 5's TLS and auth work has to cover it, not just `:8080`. Until then it is as unauthenticated as the rest of the PoC, and should not point at anything real.

## Why not the alternatives

- **Frames in `metadata`.** The chain is append-only and undeletable by design. Footage of identifiable people in a store with no delete path is a privacy failure with no remedy — and unlike a schema mistake, it cannot be superseded by a later ADR. Also gigabytes of NDJSON.
- **Base64 stills only, on motion.** Same objection, merely slower to arrive.
- **A separate "media chain".** Doubles the machinery to solve a problem hashing already solves.
- **Proxying the stream through backend-api.** Buys one origin and a future auth point, at the cost of putting a video relay in a request-scoped web app. Revisit when auth lands; not for the PoC.

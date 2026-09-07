# IOT-77: video recording, compression and 24-hour retention

**Sprint:** backlog (optional — last)
**Story points:** 5
**Status:** Deferred — optional
**Depends on:** IOT-75

## Why this is deferred
Live broadcasting (IOT-75) is the feature that matters. Recording is a separate,
optional capability on top of it.

Not building this is also the strongest privacy position available: with live-only
video, **no footage is stored at all**, so there is nothing to retain, expire,
leak or be asked to erase. The 24-hour window below only becomes a question if
recording is later wanted.

## Story
As an operator, I want recorded video kept compressed for 24 hours and then deleted so that I can review recent footage without accumulating an archive nobody meant to keep.

## Acceptance criteria
- [ ] Recorded segments stored compressed, not raw frames
- [ ] Retention enforced by an object-store lifecycle rule, not application code
- [ ] Each segment's SHA-256 and object key recorded in a ledger event
- [ ] Expired media is a normal state in the UI, visibly distinct from "missing unexpectedly"
- [ ] `GET /verify` and the panel both stay correct once media has expired
- [ ] Retention window is configurable, defaulting to 24h
- [ ] Tests cover the expired-media path, not just the happy one

## Implementation notes
- ADR 0011. Retention lives in the storage layer deliberately: a cleanup job that
  fails silently fails *open*, and the failure mode is a year of footage.
- **The hash deliberately outlives the media.** After expiry a block still names
  an object that is gone, and the block is still valid. Conflating that with
  genuine absence makes routine expiry look like tampering — and, worse,
  eventually makes real tampering look routine.
- **Check the auditor's schedule against this window.** If the auditor runs daily
  and video lives 24h, a finding late in the window can reference footage that is
  already gone. Either it runs more often, or media tied to a finding is promoted
  out of the expiring bucket before it lapses. Coordinate with IOT-69.
- Codec/bitrate is a real choice: it sets both storage cost and whether the
  footage is legible enough to be worth keeping. State what was picked and why.

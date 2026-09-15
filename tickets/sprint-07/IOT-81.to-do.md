# IOT-81: docs — firmware setup

**Sprint:** sprint-07
**Story points:** 1
**Status:** To Do
**Depends on:** IOT-30, IOT-78

## Story
As a developer, I want the hardware setup written down so that a second board can be brought up without guesswork.

## Acceptance criteria
- [ ] `docs/firmware.md`: board, wiring, pin map, flashing steps
- [ ] `secrets.h.example` fields explained
- [ ] ADR 0007's "plain HTTP, no device auth" limitation restated, now that a camera stream is exposed too

## Implementation notes
A page, not a project. Write it while the wiring is still in front of you.

Worth one line on what is actually open: the device posts over plain HTTP with
no authentication, and now serves a video stream as well. Fine for a PoC on a
desk; the thing to fix before anything real is pointed at it.

# IOT-26: Refactor `POST /events` to mpsc + oneshot

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-25

## Story
As a developer, I want the events handler to go through the writer task so that the lock-free write path is actually used.

## Acceptance criteria
- [ ] Handler builds a `WriteRequest` with a fresh `oneshot`, sends on `write_tx`, awaits the `Block`
- [ ] Returns 201 + `Block`; channel send/recv failure → 500
- [ ] Direct `RwLock` write in the old handler removed
- [ ] Existing integration tests still pass

## Implementation notes
- Reads (`/blocks`, `/verify`) keep using the `RwLock` snapshot — unchanged.
- Watch for holding locks across `.await`; the task owns state so handlers shouldn't.

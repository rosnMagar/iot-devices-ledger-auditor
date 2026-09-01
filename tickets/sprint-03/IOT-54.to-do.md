# IOT-54: Graceful shutdown on SIGTERM

**Sprint:** sprint-03
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-26

## Story
As an operator, I want `docker stop` to shut storage-core down in order, so that accepted writes are finished instead of dropped.

## Context — the shutdown path exists but is unreachable
IOT-26 added an ordered shutdown to `main.cpp`:

```
serve() returns -> queue.close() -> writer.join() -> broadcaster.close_all()
```

It is correct and it does not run. Nothing installs a signal handler, so
`SIGTERM` takes the default action and kills the process outright; `serve()`
only ever returns from a bind failure. Confirmed during IOT-26's smoke test:
after `pkill -TERM`, the "writer thread joined" line never appears.

**What that costs today:** blocks already written are safe — `append_block`
flushes every block, so the ledger file is never left torn. What is lost is
anything sitting in the `WriteQueue`: those clients are mid-`POST` and get a
reset connection for an event that will never exist. `docker stop` gives 10
seconds before `SIGKILL`, and we currently use none of it.

**Why now:** IOT-28 adds a second listener on `:8081` with its own accept loop
and per-connection threads. That is a second thing needing an orderly stop, and
retrofitting shutdown to two listeners is harder than to one.

## Acceptance criteria
- [ ] A handler for `SIGTERM` and `SIGINT` causes `serve()` to return, so the existing ordered shutdown in `main.cpp` actually runs
- [ ] In-flight requests already accepted by the queue are drained and answered, not dropped
- [ ] The WebSocket listener from IOT-28 is stopped and its connection threads joined
- [ ] `broadcaster.close_all()` wakes every subscriber thread so none is left blocked in `next()`
- [ ] Process exits 0 on a clean signal shutdown
- [ ] `docker stop` completes well inside the default 10s grace period rather than being `SIGKILL`ed
- [ ] Test: start the server, submit writes, signal it, assert every accepted write is on disk and the process exited cleanly

## Implementation notes
- **A signal handler may only call async-signal-safe functions.** It must not
  lock a mutex, allocate, or log. The standard-conforming shape is a
  `volatile std::sig_atomic_t` flag set by the handler, plus something that
  notices it. `httplib::Server::stop()` is *not* safe to call from a handler.
- Practical options, in rough order of preference:
  - a dedicated waiter thread that blocks on `sigwait` with the signal masked in
    all other threads, then calls `srv.stop()` from ordinary thread context —
    clean, and avoids the async-signal-safety problem entirely;
  - a self-pipe / `eventfd` the handler writes one byte to;
  - the flag plus a poll loop, which is the least good but simplest.
- **Order matters and is the whole point.** Stop accepting first, *then* close
  the queue, *then* join the writer, *then* close subscriptions. Closing the
  queue while handlers are still being accepted means a handler pushes onto a
  closed queue and returns 500 for a request it could have served.
- `SIGINT` too, so Ctrl-C in local dev behaves the same as `docker stop`.
- Docker sends `SIGTERM` to PID 1 only, and the shell form (`CMD foo`) makes the
  shell PID 1 so the signal never reaches the binary. **Already checked:**
  `storage-core/Dockerfile` ends in `CMD ["storage-core"]` — exec form, so the
  signal will arrive once there is something to receive it. Nothing to fix here.
- Related: the 500-vs-503 note in IOT-26. Once shutdown is orderly, a refused
  push during drain is genuinely "shutting down" and 503 becomes defensible —
  worth revisiting then, not before.

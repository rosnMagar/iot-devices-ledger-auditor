# IOT-54: Graceful shutdown on SIGTERM

**Sprint:** sprint-03
**Story points:** 3
**Status:** Review
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
- [x] A handler for `SIGTERM` and `SIGINT` causes `serve()` to return, so the existing ordered shutdown in `main.cpp` actually runs
- [x] In-flight requests already accepted by the queue are drained and answered, not dropped
- [x] The WebSocket listener from IOT-28 is stopped and its connection threads joined
- [x] `broadcaster.close_all()` wakes every subscriber thread so none is left blocked in `next()`
- [x] Process exits 0 on a clean signal shutdown
- [x] `docker stop` completes well inside the default 10s grace period rather than being `SIGKILL`ed
- [x] Test: start the server, submit writes, signal it, assert every accepted write is on disk and the process exited cleanly

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

## What was built

**`src/signals.hpp` / `signals.cpp` — `SignalWaiter`.**
Blocks SIGTERM and SIGINT process-wide, then parks one dedicated thread in
`sigwait()`. When a signal arrives that thread simply *returns* in ordinary
thread context and calls the callback.

This is what sidesteps async-signal-safety entirely. A conventional handler may
not lock a mutex, allocate, or log — which rules out everything this shutdown
does, and `httplib::Server::stop()` is not async-signal-safe either. With
`sigwait` no code ever runs on an interrupted thread's stack, so locking and
logging are fine.

Constructed **first in `run()`, before any other thread exists**: threads inherit
the mask from their creator, and a signal delivered to a thread that has not
blocked it takes the default action and kills the process. The destructor wakes
the waiter with a directed `pthread_kill` so an ordinary exit joins cleanly, and
a `winding_down_` flag keeps that wake-up from firing the callback.

**`ServerHandle` in `server.hpp`/`server.cpp`.**
`serve()` owns its `httplib::Server` as a local, so nothing outside could stop
it. `ServerHandle` hands out that ability without pulling the ~10k-line httplib
header into `main.cpp`.

It also closes a startup race: a stop arriving *before* `serve()` begins is
recorded in `stopped_`, and `attach()` reports it so the server never starts
listening. Without that, a signal during startup would be swallowed and the
process would run on forever. `detach()` clears the pointer under the same mutex
before the `Server` is destroyed, so a concurrent `stop()` either completes first
or sees null — never a dangling pointer.

**Shutdown order** in `main()`, unchanged from IOT-26/28 but now actually
reachable: stop the WS listener → close the queue → join the writer → release
subscribers. Closing the queue before the listeners stop would make in-flight
handlers return 500 for requests that could have been served.

## Testing
`tests/test_shutdown.cpp` — 6 cases / 127 assertions. The only suite that drives
the **real binary as a child process** (`fork`/`execl`, ports and ledger via
env), because the behaviour under test is what the *process* does when the
kernel delivers a signal, including the exit status `docker stop` sees.

- SIGTERM exits 0, in well under the 10s Docker grace period
- SIGINT behaves identically, so Ctrl-C matches `docker stop`
- 20 accepted writes are all on disk afterwards, in order and still hash-linked
- the ledger reloads on the next boot: restarted against the kept file,
  `/verify` valid and the last hash matches
- a live WebSocket connection does not prevent shutdown (an open socket plus a
  connection thread plus a pump thread, all of which must be joined)
- a completely idle server shuts down cleanly — the case most likely to deadlock
  on an unnecessary join

`ctest` 9/9 green; `--repeat until-fail:10` green.

**Mutation-checked.** With the `SignalWaiter` line removed from `main()`, **all 6
cases fail** — that is exactly the pre-IOT-54 behaviour, so the suite is testing
the fix rather than restating something already true. `src/main.cpp` restored and
verified clean against git.

**Measured on the real binary:** 3 events posted, `kill -TERM` → **exit 0 in
1 ms**, 4 ledger lines, and the full ordered log (`received signal 15` →
websocket listener → write queue → `writer thread joined`).

**Race-checked:** the same scenario against a `-fsanitize=thread` build of
storage-core with 5 concurrent writes — **zero ThreadSanitizer warnings** and
exit 0. Worth doing separately from the unit suites, since shutdown is where
four threads are torn down in sequence.

## Finding: leave the 500-vs-503 note alone after all
This ticket anticipated revisiting IOT-26's decision to answer **500** when
`push()` is refused, on the grounds that 503 becomes defensible once shutdown is
orderly.

Having built it: **the ordering makes that path effectively unreachable.**
`ws.stop()` and the httplib server both stop accepting *before* `queue.close()`
runs, so during a graceful shutdown there is no connection left to receive a 503.
The refused-push branch now only fires if a handler is somehow mid-flight across
the whole teardown. Changing the status code would be editing a path that no
longer occurs in the scenario that motivated the change, and would invalidate a
passing test for no observable gain. Left at 500.

## Verified, not assumed
`storage-core/Dockerfile` ends in `CMD ["storage-core"]` — exec form, so the
binary is PID 1 and receives the signal directly. Checked when the ticket was
filed; still true.

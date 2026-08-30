# IOT-44: Amend ADR 0003 — the concurrency rationale is Rust-specific

**Sprint:** sprint-03
**Story points:** 1
**Status:** Review
**Depends on:** —
**Blocks:** IOT-25

## Story
As a developer, I want ADR 0003 to state the reasoning that actually applies to the C++ implementation so that the writer-thread design is justified by something true.

## Acceptance criteria
- [x] `docs/decisions/0003-ownership-based-concurrency.md` no longer justifies the design with `.await`-specific arguments
- [x] The Consequences section states plainly that the writer thread is **not** a correctness fix in C++ — the existing `std::unique_lock` on the write path is already correct
- [x] The reasons that do survive are recorded: bounded-queue backpressure, one natural fan-out point for broadcasts, and the producer/consumer idiom as the Weekend 4 learning goal
- [x] A "Superseded by the C++ migration" note dates the change
- [x] `docs/storage-core/weekend-4-ring-buffer-ws.md` "Concepts you need" is updated too — it currently lists Tokio channels, `tokio::spawn` and `select!`

## Why
ADR 0003 currently reads:

> No lock contention or risk of holding a `Mutex` across `.await` points on the
> write path — a common Rust async pitfall.
> Demonstrates a core Tokio idiom ("ownership-based concurrency").

cpp-httplib is thread-per-connection with no async/await, so neither statement is
about this codebase. Left as-is, the repo argues for a refactor on grounds that
do not exist, and the next person to read it (including future you) can't tell
whether the writer thread is load-bearing or decorative.

It is decorative *for correctness* and worthwhile *for backpressure, fan-out, and
learning* — that distinction is the whole point of the amendment, and it should
be settled before IOT-25 is built rather than after.

## Implementation notes
- Keep the original text; append the amendment rather than rewriting history —
  the Rust reasoning is accurate for the design as originally conceived.
- Same class of rot as `docs/storage-core/weekend-3-rest-api.md`, which still
  describes axum/tokio. Worth fixing in the same pass.

## What changed
- **`docs/decisions/0003-ownership-based-concurrency.md`** — status marked amended;
  an amendment section added rather than rewriting the original, since the Rust
  reasoning is accurate for the design as first conceived. It names the two
  consequences that no longer apply (`.await` pitfall, Tokio idiom), states
  plainly that the writer thread is **not** a correctness fix in C++ because the
  existing `std::unique_lock` already serialises appends, and records the three
  reasons that do survive: bounded backpressure, a single fan-out point for the
  broadcaster, and the Weekend 4 learning goal.
- **`docs/storage-core/weekend-4-ring-buffer-ws.md`** — Goal rewritten in C++
  terms with a pointer to the amendment. "Concepts you need" replaced: Tokio
  channels, `tokio::spawn` and `select!` gave way to `std::condition_variable`
  (and why bare `wait()` is wrong), `std::promise`/`std::future`, move-only types
  in containers, and thread shutdown. Added an explicit note that async/await and
  event loops are *not* needed, since that's the most likely wrong turn for
  someone coming from the Rust design.

## A sequencing note this surfaced
If Weekend 4 is ever cut for time, **the broadcaster (IOT-27) is the only part the
live feed actually requires**. It can be built directly on the existing
`shared_mutex` without the queue or the writer thread. Recorded in the ADR so the
decision is available under time pressure rather than rediscovered then.

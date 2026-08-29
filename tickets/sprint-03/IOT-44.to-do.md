# IOT-44: Amend ADR 0003 — the concurrency rationale is Rust-specific

**Sprint:** sprint-03
**Story points:** 1
**Status:** To Do
**Depends on:** —
**Blocks:** IOT-25

## Story
As a developer, I want ADR 0003 to state the reasoning that actually applies to the C++ implementation so that the writer-thread design is justified by something true.

## Acceptance criteria
- [ ] `docs/decisions/0003-ownership-based-concurrency.md` no longer justifies the design with `.await`-specific arguments
- [ ] The Consequences section states plainly that the writer thread is **not** a correctness fix in C++ — the existing `std::unique_lock` on the write path is already correct
- [ ] The reasons that do survive are recorded: bounded-queue backpressure, one natural fan-out point for broadcasts, and the producer/consumer idiom as the Weekend 4 learning goal
- [ ] A "Superseded by the C++ migration" note dates the change
- [ ] `docs/storage-core/weekend-4-ring-buffer-ws.md` "Concepts you need" is updated too — it currently lists Tokio channels, `tokio::spawn` and `select!`

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

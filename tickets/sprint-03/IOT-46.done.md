# IOT-46: Rewrite the sprint-03 storage-core tickets for C++

**Sprint:** sprint-03
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As a developer, I want the sprint-03 storage-core tickets to describe the C++ codebase so that they can be worked without being reinterpreted first.

## Acceptance criteria
- [x] IOT-24 through IOT-29 no longer reference Rust types, crates or filenames in their criteria
- [x] Each rewritten ticket records what it was translated *from*, so the provenance isn't lost
- [x] Gaps the translation exposed are captured as their own tickets rather than buried in notes

## What was wrong
All six were written against the pre-migration Rust design, down to the type
signatures — `WriteRequest { respond_to: oneshot::Sender<Block> }`,
`mpsc::channel::<WriteRequest>(256)` created in `main.rs`,
`Arc<RwLock<Vec<Block>>>`, `broadcast::channel::<Block>(64)`, axum's
`WebSocketUpgrade`, `tokio-tungstenite` as a dev-dependency. None of it exists in
this codebase. Same rot as `docs/storage-core/weekend-3-rest-api.md`.

## Translation

| Rust | C++ |
|---|---|
| `mpsc::channel(256)` | `std::queue` + `std::mutex` + `not_empty`/`not_full` CVs |
| `oneshot::Sender<Block>` | `std::promise<Block>` / `std::future<Block>` |
| `tokio::spawn(writer_task)` | `std::thread` running `writer_loop` |
| `Arc<RwLock<Vec<Block>>>` | the `std::shared_mutex` already in `AppState` |
| `broadcast::channel(64)` | hand-built `Broadcaster` (no stdlib equivalent) |
| `RecvError::Lagged` | a `lagged_` counter on each subscription |
| axum `WebSocketUpgrade` | a vendored library on a second listener |
| `tokio::test` + tungstenite | doctest extending IOT-23's `TestServer` |

Each ticket gained the C++-specific hazards the Rust version had no reason to
mention: move-only `WriteRequest`, `set_exception` instead of throwing out of a
thread, `broken_promise` on an unfulfilled promise, calling `get_future()` before
the request is queued, and closing the queue so a parked `pop()` can be joined.

## Gaps this exposed
- **IOT-44** (1 pt) — ADR 0003 justifies the writer thread with `.await`-specific
  reasoning that doesn't apply to C++. The design is defensible on other grounds
  (backpressure, fan-out point, the Weekend 4 learning goal) but it is not the
  correctness fix the ADR claims. Blocks IOT-25.
- **IOT-45** (3 pts) — cpp-httplib has **zero** WebSocket support and nothing is
  vendored, so IOT-28 had no foundation at all. Vendoring, and proving it
  survives the arm64 build, is its own piece of work.

## The decision recorded here
`/ws/blocks` cannot be a path on the existing server: cpp-httplib and any WS
library each own a listening socket and can't share 8080. It becomes a second
server on 8081. Chosen deliberately over Server-Sent Events (which needs no
library, no second port and no security-group change) to keep the documented
WebSocket contract. The consequences — `architecture.md`, an extra public
unauthenticated port, backend-api's Phase 2 relay, the frontend's build-time URL
— are written into IOT-45.

## Follow-ups
- Sprint-03 storage-core is now 25 points, up from 21.
- `docs/storage-core/weekend-4-ring-buffer-ws.md` still lists Tokio concepts;
  folded into IOT-44.

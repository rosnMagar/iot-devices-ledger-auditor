# Weekend 3 — REST API (simple version)

Status: Not started

## Goal

Build the axum `Router` + `AppState` using `Arc<RwLock<Chain>>` directly (no mpsc yet) for `POST /events`, `GET /blocks`, `GET /verify`. Add `error.rs` (`AppError` via `thiserror` + `IntoResponse`). Integration tests via `tower::ServiceExt::oneshot`. This is a deployable milestone even if Weekend 4 slips.

## Concepts you need (read/skim before starting)

- **Async/await basics** — Tokio tutorial (tokio.rs), "Hello Tokio" + "Spawning"
- **axum routing & extractors** (`State`, `Json`, `Query`) — axum docs + examples repo
- **`Arc` and `RwLock`** for shared state across handlers — *The Rust Book*, ch. 16 (shared-state concurrency)
- *Zero To Production In Rust* (book) — parallel reading for idiomatic Rust web service structure

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD

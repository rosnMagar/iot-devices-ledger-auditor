# IOT-20: `api.rs` — Router + `AppState` (`Arc<RwLock>`) + `/health`

**Sprint:** sprint-02
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-16, IOT-18

## Story
As an API consumer, I want a running axum server with shared state so that endpoints can read/write the chain.

## Acceptance criteria
- [ ] `AppState { chain: Arc<RwLock<Chain>> }` (mpsc/broadcast added in sprint-03)
- [ ] `Router` with `GET /health` → `{"status":"ok"}`
- [ ] `TraceLayer` + `CorsLayer` applied
- [ ] Server binds to `BIND_ADDR` and serves

## Implementation notes
- Simple version first: handlers lock the `RwLock` directly. The writer-task refactor is sprint-03 (IOT-24..26).
- See `docs/storage-core/weekend-3-rest-api.md`.

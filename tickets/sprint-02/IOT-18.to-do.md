# IOT-18: `error.rs` — `AppError` + `IntoResponse`

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** —

## Story
As an API consumer, I want consistent error responses so that failures are predictable JSON.

## Acceptance criteria
- [ ] `AppError` enum via `thiserror` (e.g. `InvalidRange`, `Internal`)
- [ ] `IntoResponse` maps `InvalidRange` → 400, others → 500
- [ ] All errors return `{"error": "..."}` JSON
- [ ] Handlers can use `Result<T, AppError>` with `?`

## Implementation notes
- `anyhow` for setup/internal code; `AppError` at the handler boundary.
- Keep the public error body minimal — no internal detail leakage.

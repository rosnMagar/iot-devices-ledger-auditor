# IOT-18: `error.hpp` — `ApiError` + JSON error responses

**Sprint:** sprint-02
**Story points:** 2
**Status:** To Do
**Depends on:** —

## Story
As an API consumer, I want consistent error responses so that failures are predictable JSON.

## Acceptance criteria
- [ ] `ApiError` type carrying an HTTP status code + a public message (e.g. `InvalidRange`, `Internal`)
- [ ] A translation helper maps `ApiError` → the httplib response: `InvalidRange` → 400, others → 500
- [ ] All errors return `{"error": "..."}` JSON
- [ ] Handlers can `throw ApiError{...}` and a common wrapper sets status + body

## Implementation notes
- `ApiError` as a small struct/exception (`{ int status; std::string message; }`) derived from `std::exception`.
- Wrap each handler in a `try/catch` (or a single httplib exception handler) that catches `ApiError` → its status, and any other `std::exception` → 500 with a generic body.
- Keep the public error body minimal — no internal detail / stack leakage.

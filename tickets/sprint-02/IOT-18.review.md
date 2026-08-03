# IOT-18: `error.hpp` — `ApiError` + JSON error responses

**Sprint:** sprint-02
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As an API consumer, I want consistent error responses so that failures are predictable JSON.

## Acceptance criteria
- [x] `ApiError` type carrying an HTTP status code + a public message (e.g. `InvalidRange`, `Internal`)
- [x] A translation helper maps `ApiError` → status + body: an invalid range → 400, anything unexpected → 500
- [x] All errors return `{"error": "..."}` JSON
- [ ] Handlers can `throw ApiError{...}` and a common wrapper sets status + body — *logic done (`to_error_response`); the httplib exception-handler wiring lands with the server in IOT-20/21*

## Implementation notes
- Implemented in `storage-core/include/error.hpp` (header-only). `ApiError : std::exception { int status; std::string message; }` — constructed directly, e.g. `throw ApiError(400, "from > to")`. Kept MVP-simple: no factory helpers, the status is just the first argument.
- **Kept httplib-agnostic on purpose:** `to_error_response(const std::exception&)` returns a plain `ErrorResponse { int status; std::string body; }`, so this header carries no server dependency and lands before cpp-httplib is vendored. IOT-20/21 feed it into httplib's exception handler.
- Any non-`ApiError` exception collapses to a generic 500 (`{"error":"internal error"}`) — verified no leakage of the original `what()` text.

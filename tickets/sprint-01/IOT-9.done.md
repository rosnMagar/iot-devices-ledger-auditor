# IOT-9: `block.hpp` — `EventPayload` + `Block` structs + JSON

**Sprint:** sprint-01
**Story points:** 2
**Status:** Done
**Depends on:** —

## Story
As a developer, I want the core ledger data types so that events and blocks have a defined, serializable shape.

## Acceptance criteria
- [x] `EventPayload { event_type, location_id, actor, description, metadata }` defined
- [x] `Block { index, timestamp, event, prev_hash, hash }` defined
- [x] Both serialize via `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`; `metadata` is `nlohmann::json`
- [x] `timestamp` is `std::string` (RFC3339); `cmake --build` clean

## Implementation notes
- Replaces the Phase 0 stub in `storage-core/`.
- See `docs/storage-core/overview.md` for the exact field spec.
- `nlohmann/json` vendored at `storage-core/third_party/nlohmann/json.hpp`.

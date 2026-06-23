# IOT-9: `block.rs` — `EventPayload` + `Block` structs + serde

**Sprint:** sprint-01
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As a developer, I want the core ledger data types so that events and blocks have a defined, serializable shape.

## Acceptance criteria
- [x] `EventPayload { event_type, location_id, actor, description, metadata }` defined
- [x] `Block { index, timestamp, event, prev_hash, hash }` defined
- [x] Both derive `Debug, Clone, Serialize, Deserialize`; `metadata` is `serde_json::Value`
- [x] `timestamp` is `chrono::DateTime<Utc>`; `cargo build` clean

## Implementation notes
- Replaces the Phase 0 stub in `storage-core/`.
- See `docs/storage-core/overview.md` for the exact field spec.
- `///` doc comments encouraged this phase (learning + `cargo doc`).

# 0004 — Single crate (lib + bin), not a Cargo workspace

Status: Accepted

## Context

storage-core could be structured as a Cargo workspace (separate crates for core logic, API, etc.) or as a single crate.

## Decision

Single crate with `lib.rs` (core logic: `block`, `chain`, `storage`, `writer`, `api`, `ws`, `error` — all testable) plus a thin `main.rs` binary entrypoint (config, tracing init, spawn writer task, start server).

## Consequences

- Avoids workspace ceremony (multiple `Cargo.toml`s, inter-crate path deps) for a project of this size — appropriate for a Rust beginner.
- `lib.rs` modules are still independently unit-testable via `cargo test`.
- If storage-core later needs to be reused as a library by another Rust binary (unlikely in this project's scope), splitting into a workspace remains straightforward.

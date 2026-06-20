# Weekend 4 — Ring Buffer + WebSocket

Status: Not started

## Goal

Introduce `mpsc::channel<WriteRequest>` + `writer_task`, refactoring `POST /events` to send through it. Add `broadcast::Sender<Block>` and the `/ws/blocks` handler. Integration test: spin up the app in a `tokio::test`, connect via `tokio-tungstenite`, POST an event, assert the WS client receives the corresponding block. Capstone "ring buffer → hash chain → persist → broadcast" demo.

## Concepts you need (read/skim before starting)

- **Channels**: `mpsc`, `oneshot`, `broadcast` — Tokio tutorial chapters on channels and shared state
- **Spawned tasks** (`tokio::spawn`) and task ownership patterns
- **`select!`** macro — for handling multiple channel sources / shutdown
- axum WebSocket example (official examples repo) — `/ws` handler pattern, upgrading connections

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD

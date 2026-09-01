# IOT-53: Amend ADR 0008 — record why WebSocket rather than SSE

**Sprint:** sprint-03
**Story points:** 1
**Status:** To Do
**Depends on:** —

## Story
As a developer returning to this project, I want the choice of WebSocket for the live feed to be justified in writing, so that I find a decision rather than an assumption.

## Context — what's actually missing
ADR 0008 defends the **library** (IXWebSocket over uWebSockets and websocketpp)
and the **second listener**. Nothing defends the **protocol**.

Tracing it back: WebSocket appears in the very first commit — `34a1559`, the
Phase 0 scaffold, 2026-06-19 — already stated as fact ("REST + WebSocket API").
It was never weighed against alternatives because under the original **Rust/axum**
stack it cost nothing: `axum::extract::ws` ships with the framework.

The C++ migration (`a3521a7`) changed that calculus completely — cpp-httplib has
no WebSocket support — but the premise carried forward unexamined. ADR 0008 then
framed its decision as "keeping the documented `/ws/blocks` contract", which
preserves a choice whose original rationale no longer applies.

The transport choice is fine. The gap is that nothing says why.

## Acceptance criteria
- [ ] ADR 0008 gains a section recording why WebSocket over Server-Sent Events
- [ ] It states plainly that the original choice predates the C++ migration and was inherited from the Rust/axum design, where WebSocket was free
- [ ] It records the SSE comparison honestly: same `:8080`, no library, no second port, no security-group change, automatic reconnect with `Last-Event-ID` resumption mapping onto the block index, and one-directional — which is all a block feed needs
- [ ] It records what WebSocket buys in return: headroom for client→server messages (subscription filters, acks) without a second mechanism, and the explicit "WebSocket basics" learning goal in `docs/storage-core/weekend-4-ring-buffer-ws.md`
- [ ] It notes that the switching cost is now mostly sunk — 13,224 vendored lines, CI-green on arm64 (IOT-45), with only 7 files referencing it outside `third_party/`
- [ ] The existing "If this proves wrong" section is updated so SSE remains named as the cheap fallback

## Implementation notes
- This is a documentation-only change. No code, no behaviour.
- **Do not rewrite history in the ADR.** Record that the choice was inherited
  and later ratified, not that it was deliberated up front. An ADR that invents
  a rationale it never had is worse than one that admits the sequence.
- Keep it short. This is a paragraph and a comparison, not a second ADR.

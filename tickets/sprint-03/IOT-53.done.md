# IOT-53: Amend ADR 0008 — record why WebSocket rather than SSE

**Sprint:** sprint-03
**Story points:** 1
**Status:** Review
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
- [x] ADR 0008 gains a section recording why WebSocket over Server-Sent Events
- [x] It states plainly that the original choice predates the C++ migration and was inherited from the Rust/axum design, where WebSocket was free
- [x] It records the SSE comparison honestly: same `:8080`, no library, no second port, no security-group change, automatic reconnect with `Last-Event-ID` resumption mapping onto the block index, and one-directional — which is all a block feed needs
- [x] It records what WebSocket buys in return: headroom for client→server messages (subscription filters, acks) without a second mechanism, and the explicit "WebSocket basics" learning goal in `docs/storage-core/weekend-4-ring-buffer-ws.md`
- [x] It notes that the switching cost is now mostly sunk — 13,224 vendored lines, CI-green on arm64 (IOT-45), with only 7 files referencing it outside `third_party/`
- [x] The existing "If this proves wrong" section is updated so SSE remains named as the cheap fallback

## Implementation notes
- This is a documentation-only change. No code, no behaviour.
- **Do not rewrite history in the ADR.** Record that the choice was inherited
  and later ratified, not that it was deliberated up front. An ADR that invents
  a rationale it never had is worse than one that admits the sequence.
- Keep it short. This is a paragraph and a comparison, not a second ADR.

## What was changed
**`docs/decisions/0008-...md`** — new section "Why WebSocket at all", placed
before the Decision so it reads in order. It records the sequence honestly:
inherited from the Rust/axum scaffold (`34a1559`, 2026-06-19) where
`axum::extract::ws` made it free, carried through the C++ migration
(`a3521a7`) unexamined, and ratified here for reasons about *this project*
rather than about the protocol — sunk cost, bidirectional headroom, and the
stated learning goal. SSE is given a fair side-by-side rather than a dismissal,
including the `Last-Event-ID` resumption that maps onto our block index.

"If this proves wrong" now lists the concrete revert steps, and notes the IOT-27
broadcaster is transport-agnostic and survives either choice.

## Scope picked up along the way
ADR 0008's own Consequences section said `docs/architecture.md` "still draws it
on 8080 and needs updating when IOT-28 lands". It had landed, so that was done
here rather than left as a second stale note:

- **`architecture.md`** — the diagram gains the `:8081` listener as a separate
  node, with a dashed in-process arrow (writer → broadcaster), plus a paragraph
  saying it is a second listener rather than a path, and that nothing consumes it
  until Phase 2/3.
- **`storage-core/overview.md`** — the API table entry becomes
  `ws://<host>:8081/blocks` and documents the frame shapes, including the
  `lagged` notice. This is the table downstream code reads, so a wrong port here
  is the expensive kind of stale.
- **`backend-api.md`, `frontend.md`, roadmap Phase 2** — the consumers now name
  the right endpoint.
- **`roadmap.md`** — Weekend 4 ticked, with a note that Phase 1 is complete and
  what remains outstanding (IOT-54, and the Phase 0 `docker compose up --build`
  box).
- ADR 0008's now-satisfied "needs updating when IOT-28 lands" note replaced with
  what was actually updated.

Historical references to `/ws/blocks` — the ADR describing what was originally
specified, and the roadmap's milestone name — were deliberately left alone.
Rewriting those would be falsifying the record rather than correcting it.

## Verification
Documentation-only; no code touched and no behaviour changed. `grep -rn
"ws/blocks" docs/` leaves only the intended historical mentions.

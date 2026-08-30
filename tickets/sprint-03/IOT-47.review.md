# IOT-47: Ticket and roadmap bookkeeping

**Sprint:** sprint-03
**Story points:** 1
**Status:** Review
**Depends on:** —

## Story
As a developer, I want the board and the roadmap to reflect what has actually shipped so that project status can be read without reconstructing it from git history.

## Acceptance criteria
- [x] Every `.review` ticket whose work is merged to `prod` moved to `.done` via `git mv`
- [x] `**Status:**` inside each moved ticket updated to `Done`
- [x] `docs/roadmap.md` boxes ticked where there is evidence
- [x] Anything that can't be ticked is explained rather than left silently blank

## What was wrong
17 tickets sat in `.review` with **every acceptance criterion already ticked and
none outstanding**, all merged to `prod`. `docs/roadmap.md` had **zero** boxes
ticked — including Phase 0 items that demonstrably work. Read literally, the repo
claimed nothing had been finished.

## What changed
- 17 tickets moved `.review` → `.done`: IOT-2…5 (sprint-01), IOT-14…23 and
  IOT-41…43 (sprint-02).
- `**Status:**` normalised to `Done` across all 22 done-tickets. Two spellings
  were in use, `Review` and `In Review`; both are gone.
- Roadmap Phase 1 weekends 1–3 ticked, with the ticket ranges that delivered them.
- Roadmap Phase 0: `docs/` skeleton, dev-push CI, and prod-push deploy ticked.

## What was deliberately *not* ticked
- **`docker compose up --build` works locally** — no evidence either way this session.
- **`http://<EC2-IP>` shows the frontend stub** and **Lambda reached `/verify`** —
  same; these are IOT-7 and IOT-8, still `.to-do`.
- **"Repo default branch is `dev`"** — it is **`prod`**. Filed as IOT-48.

The four unticked Phase 0 boxes map exactly onto IOT-1, IOT-6, IOT-7 and IOT-8,
which are all still `.to-do` even though the pipeline they describe visibly
works. Closing those tickets and ticking those boxes is the same action, and it
needs someone who can confirm the frontend and Lambda by looking.

## Follow-ups
- IOT-48 — default branch is `prod`; PRs opened without `--base` target production.
- IOT-46 stays in `.review` until PR #23 merges, unlike the rest.

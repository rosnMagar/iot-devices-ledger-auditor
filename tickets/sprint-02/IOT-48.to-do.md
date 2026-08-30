# IOT-48: Reconcile the roadmap's default-branch line with the actual model

**Sprint:** sprint-02
**Story points:** 1
**Status:** To Do
**Depends on:** —

## Story
As a developer, I want the docs to agree about which branch is default so that the roadmap doesn't describe a repo that was never intended.

## Acceptance criteria
- [ ] `docs/roadmap.md` Phase 0 no longer claims the default branch should be `dev`
- [ ] The line reflects the real model: `prod` is the default and deploy branch, `dev` is the integration branch
- [ ] Box can then be ticked

## The actual situation
`docs/roadmap.md` Phase 0 lists:

> - [ ] Repo default branch is `dev`; `prod` branch exists

The repository was created 2026-06-14 with `prod` as the default and **has never
been anything else**. `docs/branching-and-cicd.md` is being updated (currently
uncommitted) to say so explicitly:

> `prod` — deploy branch and GitHub default branch.

So the repo config is intentional and the **roadmap line is the stale one**. This
ticket originally proposed changing the default branch to `dev`; that was wrong,
based on reading the roadmap without checking the branching doc.

## Consequence worth keeping in mind
With `prod` as default, `gh pr create` and the web UI target `prod` unless
`--base dev` is passed. That matches the per-ticket workflow in `CLAUDE.md`,
where prod PRs come from feature branches — but it does mean an unthinking PR
lands against production. Phase 5 already lists branch protection on `prod`,
which is the right mitigation rather than moving the default.

## Implementation notes
- Fold into the same pass as committing the pending `docs/branching-and-cicd.md`
  edit, so both files land consistent.

# IOT-48: Set the repository default branch to `dev`

**Sprint:** sprint-02
**Story points:** 1
**Status:** To Do
**Depends on:** —

## Story
As a developer, I want the repo's default branch to be `dev` so that a PR opened without an explicit base doesn't target production.

## Acceptance criteria
- [ ] GitHub default branch changed from `prod` to `dev`
- [ ] `docs/branching-and-cicd.md` states which branch is default and why
- [ ] Roadmap's "Repo default branch is `dev`" box can be ticked

## Why
`gh api repos/rosnMagar/iot-devices-ledger-auditor --jq .default_branch` returns
**`prod`**, but `docs/roadmap.md` requires `dev`. Two consequences:

- **`gh pr create` and the web UI default to base `prod`.** Every PR aimed at
  `dev` needs an explicit `--base dev` or it silently targets production. PR #20
  was created with a generic auto-title against `prod`, which is the shape this
  produces.
- **A fresh clone checks out `prod`**, so work started without thinking lands on
  the production branch.

The per-ticket workflow in `CLAUDE.md` does open prod PRs from feature branches,
so `prod` as a *target* is correct — it just shouldn't be the **default** one you
get by not specifying.

## Implementation notes
- One setting: Settings → General → Default branch, or
  `gh api -X PATCH repos/rosnMagar/iot-devices-ledger-auditor -f default_branch=dev`
- Existing open PRs keep whatever base they were created with; this only affects
  new ones.
- Worth pairing with branch protection on `prod` (Phase 5 already lists it) so the
  production branch isn't both the default *and* unprotected.

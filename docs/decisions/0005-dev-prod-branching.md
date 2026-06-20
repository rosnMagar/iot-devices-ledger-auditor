# 0005 — dev/prod branching model

Status: Accepted

## Context

The project deploys to real AWS infrastructure (EC2 + Lambda) from very early (Phase 0). In-progress work shouldn't trigger AWS deploys, but should still be continuously validated by CI and by building deployable images.

## Decision

Two long-lived branches:

- `dev` — integration branch. Feature work happens on short-lived branches, PR'd into `dev`. Pushes to `dev` run full CI and build `:dev`-tagged images to GHCR, but do **not** touch AWS.
- `prod` — deploy branch and **GitHub default branch**. Promoting `dev` → `prod` (via PR/merge) triggers CI + a full deploy: GHCR images, EC2 redeploy via SSH, and `sam deploy` for the Lambda.

## Consequences

- AWS resources (free-tier EC2, Lambda) are only touched deliberately, on promotion to `prod` — reduces risk of breaking the live demo while iterating.
- Continuous validation still happens on `dev` via CI + image builds, catching build/lint/test failures early (the original "big bang integration" risk this whole Phase 0 approach is meant to avoid).
- Requires an explicit promotion step (PR `dev` → `prod`) before any change reaches the deployed environment.

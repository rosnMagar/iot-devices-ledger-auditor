# Branching & CI/CD

## Branching model

- `dev` — integration branch. Per-ticket branches are merged here (with `--no-ff`) for testing.
- `prod` — deploy branch and GitHub default branch. Promote to `prod` via PR from the ticket's feature branch once it's greenlit.

See [`decisions/0005-dev-prod-branching.md`](decisions/0005-dev-prod-branching.md).

## Per-ticket workflow

Every ticket (see [`../tickets/`](../tickets/)) is worked on its own branch:

1. **Branch** off `dev`, named `<type>/IOT-<n>-<short-kebab-description>`:
   - `feature/` — new functionality (e.g. `feature/IOT-9-block-structs`)
   - `bugfix/` — fixing a defect (e.g. `bugfix/IOT-42-fix-verify-range`)
   - `chore/` — tooling/docs/maintenance with no app behavior change
2. **Work & commit** on the branch — commit messages use the `IOT-<n>:` prefix.
3. **Merge to `dev` with `--no-ff`** — `git merge --no-ff <branch>` always creates an explicit merge commit, so every integration is visible in `dev`'s history (never fast-forward).
4. **Test on `dev`** — the change is verified on `dev` before it can be promoted.
5. **PR to `prod`** — once greenlit, open a pull request to `prod` **from the feature branch** (not from `dev`). Merging the PR triggers the full deploy via `deploy.yml`.

The feature branch stays alive until its `prod` PR is merged, since the PR is raised from it.

## `ci.yml` — on PRs and pushes to `dev`/`prod`

Per-service jobs:

- `storage-core`: `cargo fmt --check`, `cargo clippy -- -D warnings`, `cargo test`
- `backend-api`: `ruff check`, `pytest`
- `frontend`: `npm run lint && npm run build`
- `auditor`: `npm run build && npm test`

## `deploy.yml`

- **Push to `dev`**: build & push `:dev`-tagged `linux/arm64` images to GHCR via `docker buildx`. No AWS deploy — keeps the free-tier EC2/Lambda untouched by in-progress work.
- **Push to `prod`**:
  1. Build & push `:prod`/`:latest`-tagged `linux/arm64` images to GHCR.
  2. SSH (`appleboy/ssh-action`) into EC2: `cd /opt/audit-ledger && git pull && docker compose pull && docker compose up -d`.
  3. `sam build && sam deploy` in `auditor/` to update the Lambda.

See [`decisions/0006-ghcr-and-ssh-deploy.md`](decisions/0006-ghcr-and-ssh-deploy.md).

## Required GitHub secrets

| Secret | Used for |
|---|---|
| `EC2_HOST` | SSH deploy target IP |
| `EC2_USER` | SSH user on EC2 |
| `EC2_SSH_KEY` | SSH private key for deploy |
| AWS access key/secret (IAM user) | `sam deploy` for the auditor Lambda |

`GITHUB_TOKEN` (automatic) is used for pushing images to GHCR — no extra setup needed.

## Common gotcha

`storage-core` (and other images) must be built for `linux/arm64` (Graviton) — forgetting `--platform=linux/arm64` in `docker buildx` is a classic "works locally, fails on EC2" failure mode.

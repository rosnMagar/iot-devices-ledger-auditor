# Branching & CI/CD

## Branching model

- `dev` — default/integration branch. Feature work happens on short-lived branches, PR'd into `dev`.
- `prod` — deploy branch. Promote `dev` -> `prod` via PR/merge when ready to ship.

See [`decisions/0005-dev-prod-branching.md`](decisions/0005-dev-prod-branching.md).

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

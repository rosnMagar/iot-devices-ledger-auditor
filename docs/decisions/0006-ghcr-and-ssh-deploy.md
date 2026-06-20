# 0006 — GHCR for images, SSH for EC2 deploy

Status: Accepted

## Context

Need a container registry and a deploy mechanism for a single free-tier EC2 instance, run by a solo developer.

## Decision

- Use **GitHub Container Registry (GHCR)** for built images — free for public repos, authenticates via the automatic `GITHUB_TOKEN`, no extra account/setup.
- Deploy to EC2 via **SSH** (`appleboy/ssh-action`) using secrets `EC2_HOST`, `EC2_USER`, `EC2_SSH_KEY`. The action runs `cd /opt/audit-ledger && git pull && docker compose pull && docker compose up -d`.

## Consequences

- Simplest possible path for a solo project — no AWS ECR setup, no OIDC/IAM role wiring for the EC2 deploy step.
- All images **must** be built for `linux/arm64` (Graviton) via `docker buildx --platform=linux/arm64` — a documented "works locally (amd64), fails on EC2 (arm64)" gotcha.
- SSH key in GitHub secrets is a long-lived credential — acceptable for this project's threat model, but flagged in Phase 5 polish as a candidate for tightening (e.g., dedicated deploy user, restricted command).
- The Lambda (`auditor`) still deploys via `sam build && sam deploy` using IAM-user access keys (also a Phase 5 candidate for OIDC).

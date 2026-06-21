# IOT-3: Install Docker + clone repo to `/opt/audit-ledger`

**Sprint:** sprint-01
**Story points:** 1
**Status:** To Do
**Depends on:** IOT-2

## Story
As an operator, I want Docker and the repo on the EC2 host so that `docker compose` can run the stack.

## Acceptance criteria
- [ ] Docker Engine + Compose plugin installed; `docker` works without sudo
- [ ] Repo cloned to `/opt/audit-ledger`, `prod` branch checked out
- [ ] `.env` created from `.env.example` with `TAG=prod` and `VITE_API_BASE_URL=http://<EC2-IP>:8000`

## Implementation notes
- Follow `docs/deployment.md` §2–4.
- `curl -fsSL https://get.docker.com | sh` then `usermod -aG docker ubuntu`; re-login.

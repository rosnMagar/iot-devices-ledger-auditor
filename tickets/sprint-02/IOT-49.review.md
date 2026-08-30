# IOT-49: Make the image `TAG` explicit instead of silently defaulting to `dev`

**Sprint:** sprint-02
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As an operator, I want a missing image tag to stop the deploy so that production can never quietly run development images.

## Acceptance criteria
- [x] `TAG` present in `.env.example`, blank, with an explanation of the two valid values
- [x] `docker-compose.yml` fails with a clear message when `TAG` is unset or empty, instead of defaulting
- [x] The automated deploy sets `TAG=prod` itself rather than trusting the box's `.env`
- [x] `docs/deployment.md` explains why it ships blank

## What was wrong
`docker-compose.yml` used `${TAG:-dev}` on all three images. `.env.example` had
**no `TAG` line at all**, while `docs/deployment.md` §4 says to `cp .env.example
.env` and then set `TAG=prod` — a line the reader has to know to *add*, not edit.

Miss it and every failure is silent: compose falls back to `:dev`, pulls
development images onto the production box, starts them cleanly, and the
`/health` check passes. Nothing anywhere reports a problem. This is what left the
EC2 box running the wrong images earlier this sprint.

## What changed
- **`.env.example`** — `TAG=` added, deliberately blank, with a comment naming
  `dev` and `prod` and pointing at `docs/deployment.md`.
- **`docker-compose.yml`** — `${TAG:-dev}` → `${TAG:?TAG is not set — set it in
  .env (dev or prod), see docs/deployment.md}` on all three services. `:?` fires
  on **empty as well as unset**, so copying `.env.example` without editing it
  fails loudly rather than picking a tag for you.
- **`.github/workflows/deploy.yml`** — the SSH script now `export TAG=prod`
  before `docker compose pull`. A command-line value overrides `.env`, so a
  missing or stale `.env` on the box can no longer misdirect a production deploy
  at all. This is the part that actually closes the hole; the guard above only
  helps someone running compose by hand.
- **`docs/deployment.md`** — explains the blank default and that the automated
  deploy sets the tag itself.

## Verified
`docker compose` is not installed on this dev machine, so this was **not**
validated with `docker compose config`. The substitution was verified against the
shell's `${VAR:?err}`, which is the same POSIX form compose implements:

| `TAG` | Result |
|---|---|
| unset | fails: `TAG is not set — set it in .env (dev or prod)…` |
| `""` (blank line in `.env`) | fails, same message |
| `prod` | `ghcr.io/rosnmagar/storage-core:prod` |
| `dev` | `ghcr.io/rosnmagar/storage-core:dev` |

**Worth running `docker compose config` on a machine that has it before merging**
— the guard is only useful if compose parses it, and a typo here breaks every
service at once.

## Follow-ups
- The same silent-default pattern applies to `VITE_API_BASE_URL`, which is baked
  into the frontend image at build time and defaults to `http://localhost:8000`.
  A production frontend built without it points at the viewer's own machine.
- Nothing verifies *which* tag is actually running. A `GET /health` that returned
  the image tag or git SHA would make "is the right build deployed?" answerable
  without SSHing in.

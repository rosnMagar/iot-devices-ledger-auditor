# IOT-57: deploy exports TAG/CORS_ORIGINS instead of persisting them

**Sprint:** sprint-04
**Story points:** 2
**Status:** Review
**Depends on:** IOT-49, IOT-51

## Story
As an operator, I want a manual `docker compose up` to produce the same configuration the deploy did, so that restarting a service during an incident does not silently change how it behaves.

## What happened
The production frontend started failing every request:

```
Access to fetch at 'http://<host>:8000/users' from origin 'http://<host>'
has been blocked by CORS policy: No 'Access-Control-Allow-Origin' header
is present on the requested resource.
```

`deploy.yml` set the value like this:

```bash
export CORS_ORIGINS=http://${{ secrets.EC2_HOST }}
```

That is a shell variable in the deploy's SSH session. **It was never written to
`.env`.** It existed only for the `docker compose up -d` on the next line and
died with the session.

`docker-compose.yml` then falls back:

```yaml
CORS_ORIGINS: ${CORS_ORIGINS:-http://localhost:5173}
```

So any `docker compose up` run outside a deploy — restarting a service by hand
during an incident, which is exactly when it happens — brought the container up
with the Vite dev server as its allowed origin. backend-api still answered 200;
the *browser* discarded every response. Nothing in any log said anything was
wrong.

Nothing changed on the box and nothing "reverted". The correct value had never
been persisted, so the box was always one manual restart away from this.

## The asymmetry that kept it hidden
IOT-49 changed `TAG` to `${TAG:?...}`, so a manual restart **fails loudly** with
"TAG is not set". `CORS_ORIGINS` kept `:-` with a localhost default, so the same
restart **succeeds quietly** and serves the wrong origin.

Same class of bug, one variable fixed and the other not — this is IOT-51
recurring through a path IOT-51 did not close.

## Acceptance criteria
- [x] The deploy writes `TAG` and `CORS_ORIGINS` into `/opt/audit-ledger/.env`
- [x] Rewriting is idempotent — repeated deploys do not duplicate lines
- [x] Existing unrelated keys in `.env` are preserved
- [x] The deploy fails if the values were not persisted
- [x] A manual `docker compose up` produces the same config the deploy did

## The fix
`deploy.yml` now deletes any existing `TAG=` / `CORS_ORIGINS=` lines and appends
the intended values before `docker compose up`:

```bash
touch .env
sed -i '/^TAG=/d;/^CORS_ORIGINS=/d' .env
printf 'TAG=%s\n' 'prod' >> .env
printf 'CORS_ORIGINS=%s\n' 'http://<EC2_HOST>' >> .env
```

They are still exported as well, so this deploy does not depend on compose
re-reading the file it was just handed.

A guard then fails the deploy if either value is missing from `.env`, since a
deploy that silently leaves the box one restart from breakage is the thing being
fixed. The guard greps `.env` directly rather than `docker compose config`, so it
does not depend on compose's output formatting — which could not be verified
locally, and a wrong guess there would have failed every deploy.

## Testing
The rewrite logic was exercised against three `.env` shapes:
- **absent/empty** → both keys written
- **re-run** → no duplicate lines
- **stale values plus unrelated keys** (`TAG=dev`, `CORS_ORIGINS=http://localhost:5173`,
  `LOG_LEVEL`, `DATABASE_URL`) → both replaced, the unrelated keys preserved

The guard was tested both ways: passes on a correct `.env`, fails on a wrong one.
`deploy.yml` parses as valid YAML with all three jobs intact.

Not verifiable locally: the actual SSH deploy. This box has no `docker compose`
plugin, so the end-to-end path is exercised by the next deploy to prod.

## Follow-up
`docs/deployment.md` records the public IP as `52.15.229.12`; the address in the
CORS error was `3.16.105.105`. If the Elastic IP from IOT-50 was released or the
instance replaced, `EC2_HOST` needs updating too — otherwise the next deploy
writes the wrong origin into `.env` and bakes the wrong URL into the frontend
bundle. Not changed here, since the current address is the operator's to confirm.

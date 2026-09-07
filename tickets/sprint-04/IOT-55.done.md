# IOT-55: backend-api crash-loops — no data directory, no volume

**Sprint:** sprint-04
**Story points:** 2
**Status:** Review
**Depends on:** IOT-34

## Story
As an operator, I want backend-api to start and keep its data across redeploys, so that the dashboard is not down and registered devices are not wiped on every deploy.

## What happened
After IOT-34 deployed, `backend-api` on EC2 entered a restart loop:

```
Error response from daemon: Container 95b99df6... is restarting,
wait until the container is running
```

Root cause, reproduced locally against the shipped image:

```
sqlalchemy.exc.OperationalError: (sqlite3.OperationalError) unable to open database file
ERROR:    Application startup failed. Exiting.
```

`DATABASE_URL` defaults to `sqlite:////app/data/app.db`, but:
- the **image never created `/app/data`** — the Dockerfile only does `COPY app ./app`
- **`docker-compose.yml` mounted no volume** on backend-api; `storage-core` has
  `ledger-data:/app/data`, backend-api had nothing

SQLite does not create missing parent directories, so the open failed, the
lifespan hook raised, uvicorn exited, and `restart: unless-stopped` looped it.

**The misconfiguration was pre-existing.** `DATABASE_URL` has pointed at a
directory that never existed since the Phase 0 scaffold. Nothing had ever opened
the database, so it sat there invisible until IOT-34 gave it a reason to. IOT-34
did not introduce the bug; it was the change that finally exercised it.

## Acceptance criteria
- [x] backend-api starts with the stock `DATABASE_URL` and no volume mounted
- [x] The SQLite parent directory is created if missing
- [x] The image owns `/app/data` so a bare `docker run` works
- [x] `docker-compose.yml` mounts a named volume so the database survives a redeploy
- [x] Regression test covering the missing-directory case
- [x] `ruff check` and `pytest` green

## The fix, in three layers
1. **`app/db.py`** — `_ensure_sqlite_directory()` creates the parent directory
   for file-backed SQLite URLs before `create_all`. No-ops for `:memory:`, for
   a pathless `sqlite://`, and for non-SQLite backends. This is the layer that
   matters: it means the app comes up on any sane path instead of depending on
   the image and the compose file agreeing about a directory neither obviously
   owns.
2. **`backend-api/Dockerfile`** — `RUN mkdir -p /app/data`, so a bare
   `docker run` with no volume works.
3. **`docker-compose.yml`** — a `backend-data:/app/data` named volume.

## The second bug, which had not bitten yet
Even once it started, **the database was in the container's writable layer** and
would have been destroyed on every `docker compose up` with a new image. Empty
tables today, so nothing was lost — but the first registered device would have
vanished at the next deploy, and that failure would have looked like an
application bug rather than a missing volume. The named volume fixes it now,
while there is no data to lose.

## Verification
- `ruff check .` clean; `pytest` **18 passed** (2 new)
- **The exact configuration that crash-looped** — `DATABASE_URL=sqlite:////app/data/app.db`,
  no volume — now starts: `/health` 200 and all four tables present
- A deliberately deeper missing path (`/var/lib/iot/nested/app.db`) also works
- The pre-fix image reproduces the original failure, so the fix is doing the work

## Follow-up worth considering
The same class of failure — a startup crash under `restart: unless-stopped`,
where the real error scrolls past in whichever restart you happen to read — will
recur. A healthcheck in `docker-compose.yml` would at least make
`docker compose ps` say *unhealthy* rather than *restarting*, and
`docker compose logs --tail` is the diagnostic worth putting in the runbook.
Not done here to keep the fix tight.

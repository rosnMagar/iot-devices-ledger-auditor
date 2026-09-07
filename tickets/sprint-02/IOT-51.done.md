# IOT-51: Fix CORS rejecting the production frontend

**Sprint:** sprint-02
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As a user of the deployed dashboard, I want the frontend to be able to call backend-api so that the page shows data instead of failing silently in the browser console.

## Acceptance criteria
- [x] The deploy sets `CORS_ORIGINS` from `EC2_HOST` rather than relying on the box's `.env`
- [x] Whitespace around comma-separated origins no longer breaks matching
- [x] `.env.example` states that the default is local-only
- [x] `docs/deployment.md` covers it

## The bug
`backend-api/app/main.py` defaulted `CORS_ORIGINS` to `http://localhost:5173` —
the Vite dev server. On the EC2 box the frontend is served by nginx on **port
80**, so the browser sends `Origin: http://<EC2-IP>` when calling backend-api on
`:8000`. Different port means a cross-origin request; that origin was not in the
allow list; FastAPI's `CORSMiddleware` omitted `Access-Control-Allow-Origin`; the
browser blocked the response.

Nothing logs an error for this. backend-api returns 200 with a valid body — the
*browser* discards it. Server-side everything looks healthy, which is why it
survived to production.

`docs/deployment.md` §4 lists `TAG` and `VITE_API_BASE_URL` as the values to set
on the box. `CORS_ORIGINS` was never mentioned, so the dev-server default shipped.

## Same shape as IOT-49
A variable with a convenient local default, no production value anywhere in the
runbook, and a failure mode that produces no server-side error. Third instance
this sprint: `TAG` (IOT-49), and `VITE_API_BASE_URL` is still outstanding.

## What changed
- **`.github/workflows/deploy.yml`** — the SSH script exports
  `CORS_ORIGINS=http://${{ secrets.EC2_HOST }}` next to `TAG=prod`. Derived from
  the same secret that already feeds `FRONTEND_API_URL`, so the allowed origin
  cannot drift from the address actually serving the page.
- **`backend-api/app/main.py`** — origins are stripped and blanks dropped.
  `"a, b".split(",")` yields `" b"`, which matches nothing and presents as a CORS
  bug with no error anywhere. Fails closed on empty, which is the right direction
  for CORS.
- **`.env.example`** — the default is documented as local-only.
- **`docs/deployment.md`** — added to the runbook.

## Verified
Origin parsing checked directly:

| `CORS_ORIGINS` | Parsed |
|---|---|
| unset | `['http://localhost:5173']` |
| `http://1.2.3.4` | `['http://1.2.3.4']` |
| `http://1.2.3.4, http://localhost:5173` | `['http://1.2.3.4', 'http://localhost:5173']` |
| `http://a,,http://b` | `['http://a', 'http://b']` |
| `"  "` | `[]` — fails closed |

`app/main.py` compiles. **Not verified end-to-end against a browser** — that needs
the deploy to run. Confirm after merging with:

```bash
curl -si -H "Origin: http://<EC2-IP>" http://<EC2-IP>:8000/blocks | grep -i access-control
```

An `access-control-allow-origin` header matching the origin means it's fixed.

## Follow-ups
- `VITE_API_BASE_URL` has the identical problem and is still unfixed — it defaults
  to `http://localhost:8000` and is baked into the frontend image at build time.
- After IOT-50 (Elastic IP), the origin stops moving. Until then every instance
  restart invalidates this value too, because it derives from `EC2_HOST`.
- Phase 5 should reconsider the whole shape: a reverse proxy serving frontend and
  API from one origin removes the CORS question entirely.

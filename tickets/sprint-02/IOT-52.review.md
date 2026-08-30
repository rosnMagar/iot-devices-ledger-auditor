# IOT-52: Remove the silent localhost fallback from `VITE_API_BASE_URL`

**Sprint:** sprint-02
**Story points:** 2
**Status:** Review
**Depends on:** —

## Story
As an operator, I want a frontend built without an API address to say so, so that a misconfigured production build isn't indistinguishable from a working one.

## Acceptance criteria
- [x] No layer defaults `VITE_API_BASE_URL` to `http://localhost:8000`
- [x] Building the image without it fails, rather than producing a broken bundle
- [x] A bundle built without it renders a clear message instead of calling the viewer's machine
- [x] An empty `EC2_HOST` fails the prod build instead of baking `http://:8000`
- [x] `.env.example` and `docs/deployment.md` explain the build-time coupling

## The bug
`http://localhost:8000` was the default in **four** separate places:

| Layer | Line |
|---|---|
| `frontend/Dockerfile` | `ARG VITE_API_BASE_URL=http://localhost:8000` |
| `docker-compose.yml` | `${VITE_API_BASE_URL:-http://localhost:8000}` |
| `frontend/src/App.tsx` | `?? 'http://localhost:8000'` |
| `.env.example` | `VITE_API_BASE_URL=http://localhost:8000` |

Any one of them produces a production frontend that calls **the viewer's own
machine**. Every visitor gets a connection error against their own localhost;
the server logs nothing because no request ever reaches it.

Worse than the `TAG` and `CORS_ORIGINS` bugs before it: Vite inlines this into
the JS bundle at build time, so a wrong value cannot be fixed by restarting or
by editing `.env` on the box. It needs an image rebuild. The symptom therefore
outlives the obvious remedies.

## Third instance of one pattern
`TAG` (IOT-49), `CORS_ORIGINS` (IOT-51), and now this: a convenient local default,
no production value in the runbook, and a failure mode with no server-side error.

## What changed
- **`frontend/Dockerfile`** — `ARG` has no default, and the build asserts the
  value is non-empty before `npm run build`. A missing build arg now fails the
  image build instead of producing a quietly wrong bundle.
- **`frontend/src/App.tsx`** — the `?? 'http://localhost:8000'` fallback is gone.
  When the value is absent the page renders an explanation, including that a
  rebuild — not a restart — is what fixes it.
- **`docker-compose.yml`** — `:-` → `:?`, matching the `TAG` treatment from IOT-49.
- **`.github/workflows/deploy.yml`** — the prod branch fails if `EC2_HOST` is
  empty, which would otherwise bake the malformed `http://:8000` into the bundle.
- **`.env.example`** — documents that this is build-time, not runtime.

## Verified
`npx tsc --noEmit` clean. Real Vite builds, both ways:

| Build | Result |
|---|---|
| `VITE_API_BASE_URL=http://1.2.3.4:8000 npm run build` | `http://1.2.3.4:8000` present in `dist/assets/*.js` |
| unset | **no `http://localhost:8000` anywhere in the bundle** |

The second case is the one that matters: previously the bundle silently carried
localhost. Now it carries nothing and the app says why.

Not verified in a browser end-to-end — that needs a deploy.

## Follow-ups
- **The real fix is runtime configuration.** Serving a small generated
  `config.js` that nginx writes at container start would decouple the image from
  the environment entirely: one image promoted dev → prod, no rebuild when the
  address changes, and `EC2_HOST` would stop being a build-time input. Worth
  doing before Phase 3 builds a real dashboard on top of this.
- Until then, every `EC2_HOST` change requires a frontend rebuild — which is why
  IOT-50's Elastic IP matters for this ticket too.

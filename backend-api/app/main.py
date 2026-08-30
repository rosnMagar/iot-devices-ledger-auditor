import os

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="backend-api (stub)")

# Comma-separated list of allowed browser origins. The default is the Vite dev
# server and is only ever right locally — in production this is set by the
# deploy workflow from EC2_HOST. Entries are stripped because "a, b" would
# otherwise yield " b", which silently matches no origin and reads as a CORS
# bug with no error anywhere in the logs.
_cors_origins = [
    origin.strip()
    for origin in os.environ.get("CORS_ORIGINS", "http://localhost:5173").split(",")
    if origin.strip()
]
app.add_middleware(
    CORSMiddleware,
    allow_origins=_cors_origins,
    allow_methods=["*"],
    allow_headers=["*"],
)

STORAGE_CORE_URL = os.environ.get("STORAGE_CORE_URL", "http://localhost:8080")


@app.get("/health")
async def health():
    return {"status": "ok"}


@app.get("/blocks")
async def blocks():
    # Real proxy to storage-core — validates inter-container networking in Phase 0.
    async with httpx.AsyncClient() as client:
        try:
            resp = await client.get(f"{STORAGE_CORE_URL}/blocks", timeout=5.0)
            resp.raise_for_status()
            return resp.json()
        except httpx.RequestError as exc:
            raise HTTPException(status_code=502, detail=f"storage-core unreachable: {exc}")


@app.get("/users")
async def users():
    # Stub — Phase 2 replaces this with real SQLAlchemy queries.
    return [
        {"id": 1, "username": "operator-1", "role": "operator"},
        {"id": 2, "username": "admin", "role": "admin"},
    ]

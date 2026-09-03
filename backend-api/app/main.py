import os
from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from app.db import init_db


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Create any missing tables before the first request is served.

    A lifespan handler rather than @app.on_event("startup"), which FastAPI
    deprecates.

    create_all is create-if-absent: it never alters an existing table, so a
    changed column will not appear on a database that already has it. Fine while
    the schema is still moving and the data is disposable — see
    docs/db-schema.md for why the Postgres move needs Alembic instead.
    """
    init_db()
    yield


app = FastAPI(title="backend-api (stub)", lifespan=lifespan)

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
    # Still a stub. IOT-34 added the models but deliberately did not switch this
    # over: the users table starts empty, so a real query would return [] and
    # blank the frontend that currently renders these two rows. The users
    # endpoint moves across with the auth work, which is what will populate it.
    return [
        {"id": 1, "username": "operator-1", "role": "operator"},
        {"id": 2, "username": "admin", "role": "admin"},
    ]

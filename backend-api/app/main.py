import os
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Annotated

import httpx
from fastapi import Depends, FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.activity import ACTIVE_WINDOW_SECONDS, LedgerActivity
from app.db import get_session, init_db
from app.models import Device


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Lifespan, not @app.on_event("startup") — FastAPI deprecates that.
    init_db()
    yield


app = FastAPI(title="backend-api (stub)", lifespan=lifespan)

# One cache per process; safe to share across requests.
ledger_activity = LedgerActivity()

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


STATUS_VALUES = ("active", "inactive", "all")
SORT_KEYS = ("last_seen", "name", "location")
ORDER_VALUES = ("asc", "desc")

# Never-reported devices sort as the oldest possible time, so they land at the
# bottom under the default last_seen/desc.
_NEVER = datetime.min.replace(tzinfo=timezone.utc)


def _one_of(value: str, allowed: tuple[str, ...], param: str) -> str:
    if value not in allowed:
        raise HTTPException(
            status_code=400,
            detail=f"{param} must be one of {', '.join(allowed)} (got {value!r})",
        )
    return value


def _sort_key(record: dict, sort: str):
    # device_id is the tiebreaker everywhere, so results are deterministic.
    if sort == "last_seen":
        return (record["last_seen"] or _NEVER, record["device_id"])
    if sort == "location":
        return (record["location_id"] or "", record["device_id"])
    return (record["device_id"], "")  # "name" — devices have no separate name


@app.get("/devices")
async def devices(
    session: Annotated[Session, Depends(get_session)],
    status: str = "all",
    location_id: str | None = None,
    device_type: str | None = None,
    sort: str = "last_seen",
    order: str = "desc",
):
    # Registry from SQLite; status/last_seen derived from the ledger.
    # location_id/device_type filter in SQL (both indexed); status and every sort
    # run in Python because they depend on derived values SQL cannot see.
    _one_of(status, STATUS_VALUES, "status")
    _one_of(sort, SORT_KEYS, "sort")
    _one_of(order, ORDER_VALUES, "order")

    await ledger_activity.refresh()

    query = select(Device)
    if location_id is not None:
        query = query.where(Device.location_id == location_id)
    if device_type is not None:
        query = query.where(Device.device_type == device_type)

    records = [
        {
            "device_id": device.device_id,
            "location_id": device.location_id,
            "device_type": device.device_type,
            "registered_at": device.registered_at,
            # null = never reported; a timestamp = went quiet.
            "last_seen": ledger_activity.last_seen(device.device_id),
            "status": ledger_activity.status(device.device_id),
        }
        for device in session.execute(query).scalars().all()
    ]

    if status != "all":
        records = [r for r in records if r["status"] == status]

    records.sort(key=lambda r: _sort_key(r, sort), reverse=(order == "desc"))

    return {
        "devices": records,
        "count": len(records),
        "filters": {
            "status": status,
            "location_id": location_id,
            "device_type": device_type,
        },
        "sort": sort,
        "order": order,
        "active_window_seconds": ACTIVE_WINDOW_SECONDS,
        "ledger_reachable": ledger_activity.reachable,
    }


@app.get("/users")
async def users():
    # Still a stub: the users table starts empty, so a real query would blank
    # the frontend. Moves across with the auth work that populates it.
    return [
        {"id": 1, "username": "operator-1", "role": "operator"},
        {"id": 2, "username": "admin", "role": "admin"},
    ]

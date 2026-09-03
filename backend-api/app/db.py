# Engine, session and schema creation. SQLite via DATABASE_URL; models stay
# DB-agnostic so Postgres is a URL change plus Alembic (docs/db-schema.md).

import os
from collections.abc import Iterator
from datetime import datetime, timezone
from pathlib import Path

from sqlalchemy import DateTime, Engine, create_engine, event
from sqlalchemy.engine import make_url
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker
from sqlalchemy.types import TypeDecorator

DATABASE_URL = os.environ.get("DATABASE_URL", "sqlite:///./app.db")


class UtcDateTime(TypeDecorator):
    # SQLite has no timezone type and returns naive datetimes; Postgres returns
    # aware ones. Normalise both ends so the two backends agree.
    impl = DateTime(timezone=True)
    cache_ok = True

    def process_bind_param(self, value: datetime | None, dialect) -> datetime | None:
        if value is None:
            return None
        if value.tzinfo is None:
            return value.replace(tzinfo=timezone.utc)
        return value.astimezone(timezone.utc)

    def process_result_value(self, value: datetime | None, dialect) -> datetime | None:
        if value is None:
            return None
        if value.tzinfo is None:
            return value.replace(tzinfo=timezone.utc)
        return value.astimezone(timezone.utc)


class Base(DeclarativeBase):
    pass


def _engine_kwargs(url: str) -> dict:
    # check_same_thread is SQLite-only and Postgres rejects it. FastAPI runs sync
    # endpoints in a threadpool, so the guard blocks correct code.
    if url.startswith("sqlite"):
        return {"connect_args": {"check_same_thread": False}}
    return {}


engine = create_engine(DATABASE_URL, **_engine_kwargs(DATABASE_URL))

SessionLocal = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False)


@event.listens_for(Engine, "connect")
def _enable_sqlite_foreign_keys(dbapi_connection, connection_record) -> None:
    # SQLite ships with foreign keys OFF, so constraints would be real in
    # production and decorative in development. Listener is on Engine so test
    # engines get it too.
    if type(dbapi_connection).__module__.startswith("sqlite3"):
        cursor = dbapi_connection.cursor()
        cursor.execute("PRAGMA foreign_keys=ON")
        cursor.close()


def _ensure_sqlite_directory(url: str) -> None:
    # SQLite will not create missing parent directories; it fails with "unable to
    # open database file". From a startup hook that is a crash loop (IOT-55).
    parsed = make_url(url)
    if not parsed.drivername.startswith("sqlite"):
        return
    if not parsed.database or parsed.database == ":memory:":
        return
    parent = Path(parsed.database).parent
    if parent and not parent.exists():
        parent.mkdir(parents=True, exist_ok=True)


def init_db() -> None:
    # create_all is create-if-absent: it never alters an existing table, so a
    # changed column will not appear on a database that already has it.
    _ensure_sqlite_directory(DATABASE_URL)

    from app import models  # noqa: F401  — registers models on Base.metadata

    Base.metadata.create_all(bind=engine)


def get_session() -> Iterator[Session]:
    session = SessionLocal()
    try:
        yield session
    finally:
        session.close()

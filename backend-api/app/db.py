"""Database engine, session and schema creation.

SQLite for now, via ``DATABASE_URL``. The models stay DB-agnostic so the
Postgres move is a URL change plus a migration tool — see ``docs/db-schema.md``.
"""

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
    """A datetime that is always timezone-aware UTC, on every backend.

    Backends disagree, and the disagreement is silent. Postgres ``TIMESTAMPTZ``
    hands back an aware datetime; **SQLite has no timezone type at all** and
    returns a naive one, even for a column declared ``DateTime(timezone=True)``
    and even when an aware value was stored.

    Left alone, that means the same expression compares correctly against
    ``datetime.now(timezone.utc)`` in production and raises
    ``TypeError: can't compare offset-naive and offset-aware datetimes`` in
    development — or worse, silently misorders rows once anything crosses a
    timezone. Development and production must not differ here, so this
    normalises both ends:

    - **on the way in**: a naive value is assumed UTC; an aware one is converted
      to UTC. Only UTC is ever stored.
    - **on the way out**: a naive value read back has UTC attached.

    On Postgres this is effectively a pass-through; on SQLite it is what makes
    the contract hold.
    """

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
    """Declarative base for every model in :mod:`app.models`."""


def _engine_kwargs(url: str) -> dict:
    """Connect args that only make sense for SQLite.

    ``check_same_thread`` is a SQLite-only guard that forbids using a connection
    from a thread other than the one that opened it. FastAPI runs sync endpoints
    in a threadpool, so a request can legitimately land on a different thread;
    SQLAlchemy's pool already serialises access, so the guard blocks correct code.

    Kept behind a URL check rather than passed unconditionally, because Postgres
    rejects the argument outright — this is the single place the code is not
    DB-agnostic, and it fails loudly rather than silently if that changes.
    """
    if url.startswith("sqlite"):
        return {"connect_args": {"check_same_thread": False}}
    return {}


engine = create_engine(DATABASE_URL, **_engine_kwargs(DATABASE_URL))

SessionLocal = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False)


@event.listens_for(Engine, "connect")
def _enable_sqlite_foreign_keys(dbapi_connection, connection_record) -> None:
    """Turn on foreign key enforcement for SQLite.

    SQLite ships with foreign keys **disabled** — without this, a device can
    reference a location that does not exist and nothing complains. Every other
    supported database enforces them by default, so leaving this off would mean
    the constraints in :mod:`app.models` are real in production and decorative
    in development, which is the worst of both.

    Registered on ``Engine`` rather than our own engine so test engines built
    with an in-memory URL get it too.
    """
    # Guard on the driver, not the URL: this listener sees every engine, and
    # PRAGMA is meaningless to anything but SQLite.
    if type(dbapi_connection).__module__.startswith("sqlite3"):
        cursor = dbapi_connection.cursor()
        cursor.execute("PRAGMA foreign_keys=ON")
        cursor.close()


def _ensure_sqlite_directory(url: str) -> None:
    """Create the directory a SQLite file lives in, if it does not exist.

    SQLite will not create missing parent directories: pointed at
    ``/app/data/app.db`` with no ``/app/data``, it fails with the notably
    unhelpful ``unable to open database file``. Raised from a startup hook that
    becomes a crash loop under ``restart: unless-stopped``, with the real cause
    buried in whichever restart you happen to read.

    IOT-55: exactly that took production down. The container had no volume
    mounted at ``/app/data``, and nothing had ever opened the database before
    IOT-34 added models, so the misconfiguration sat there invisible.

    Creating the directory here means the app comes up on any sane path rather
    than depending on the image and the compose file agreeing about a directory
    neither of them obviously owns.
    """
    parsed = make_url(url)
    if not parsed.drivername.startswith("sqlite"):
        return
    # In-memory databases have no path, and ":memory:" is not a filename.
    if not parsed.database or parsed.database == ":memory:":
        return

    parent = Path(parsed.database).parent
    if parent and not parent.exists():
        parent.mkdir(parents=True, exist_ok=True)


def init_db() -> None:
    """Create any missing tables.

    ``create_all`` is create-if-absent only: it never alters or drops an
    existing table, so a column added to a model will *not* appear on a database
    that already has that table. That is fine while the schema is still being
    shaped and the data is disposable; it is exactly why the Postgres move needs
    a real migration tool (see ``docs/db-schema.md``).
    """
    _ensure_sqlite_directory(DATABASE_URL)

    # Imported for the side effect of registering the models on Base.metadata.
    # Without this, create_all() finds nothing when the caller has not already
    # imported app.models.
    from app import models  # noqa: F401

    Base.metadata.create_all(bind=engine)


def get_session() -> Iterator[Session]:
    """FastAPI dependency yielding a session that is always closed."""
    session = SessionLocal()
    try:
        yield session
    finally:
        session.close()

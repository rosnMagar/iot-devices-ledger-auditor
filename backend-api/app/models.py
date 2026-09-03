"""Relational models for backend-api.

## Where the source of truth lives

These tables are a **registry**, not a record of what happened. Events are the
ledger's job: storage-core owns an append-only hash chain and nothing here may
contradict it. So `devices` and `locations` describe what *is registered*, while
anything about what a device *did* — last seen, reading counts, activity — is
derived by reading the ledger (IOT-35).

## Why the primary keys are strings

`locations.id` and `devices.device_id` are natural string keys that match the
ledger verbatim. A `SENSOR_READING` block carries `location_id` and `actor` as
strings the firmware reads from its `secrets.h` (see `docs/firmware.md`), so
using the same values here makes correlating a registry row to ledger events a
direct key match with no translation table.

The alternative — integer surrogate keys plus a unique slug — is the more
conventional relational shape and would let a location be renamed without
touching firmware. It was rejected because the ledger is immutable: historical
blocks can never be rewritten to point at a new key, so the mapping layer would
have to exist forever and buy nothing.

The cost is real and worth stating: renaming a location means re-flashing the
devices that report to it, and a typo in `secrets.h` produces events that match
no registered location. IOT-35 has to decide what to do with those orphans.
"""

from datetime import datetime, timezone

from sqlalchemy import ForeignKey, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db import Base, UtcDateTime


def _utcnow() -> datetime:
    """Timezone-aware UTC now.

    ``datetime.utcnow`` is deprecated in 3.12 and returns a *naive* datetime,
    which compares as if it were local time and silently misorders rows once
    anything crosses a timezone. Everything stored here is UTC and says so.
    """
    return datetime.now(timezone.utc)


class User(Base):
    """An operator of the dashboard.

    **No authentication is implemented yet.** ``password_hash`` exists because
    the schema sketch called for it, but nothing writes or checks it, and no
    endpoint authenticates. The auth ticket must pick a real KDF (argon2 or
    bcrypt) and store only its output — never a plaintext password, and never a
    bare SHA of one.
    """

    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    password_hash: Mapped[str | None] = mapped_column(String(255), default=None)
    role: Mapped[str] = mapped_column(String(32), default="operator")
    created_at: Mapped[datetime] = mapped_column(UtcDateTime, default=_utcnow)

    def __repr__(self) -> str:
        return f"<User {self.username!r} role={self.role!r}>"


class Location(Base):
    """A physical place devices report from.

    ``id`` is the same string the firmware puts in a block's ``location_id``.
    """

    __tablename__ = "locations"

    id: Mapped[str] = mapped_column(String(64), primary_key=True)
    name: Mapped[str] = mapped_column(String(128))
    description: Mapped[str | None] = mapped_column(Text, default=None)

    devices: Mapped[list["Device"]] = relationship(
        back_populates="location", cascade="save-update"
    )

    def __repr__(self) -> str:
        return f"<Location {self.id!r} name={self.name!r}>"


class Setting(Base):
    """App-wide key/value configuration — alert thresholds and the like.

    Global rather than per-location, which resolves the open question in
    ``docs/db-schema.md`` for now. Per-location overrides would mean a nullable
    ``location_id`` and a lookup that falls back to the global row; that is
    easy to add later and pointless to build before something needs it.
    """

    __tablename__ = "settings"

    id: Mapped[int] = mapped_column(primary_key=True)
    key: Mapped[str] = mapped_column(String(128), unique=True, index=True)
    value: Mapped[str] = mapped_column(Text)
    updated_at: Mapped[datetime] = mapped_column(
        UtcDateTime, default=_utcnow, onupdate=_utcnow
    )

    def __repr__(self) -> str:
        return f"<Setting {self.key!r}>"


class Device(Base):
    """A registered IoT device.

    ``device_id`` is the same string the firmware sends as a block's ``actor``.
    Registration here is what makes a device *known*; it says nothing about
    whether it is currently reporting. "Active" is derived from ledger activity
    in IOT-35 and deliberately is not a column — a stored flag would need
    something to keep it true, and would drift the moment that failed.

    ``location_id`` is nullable so a device can be registered before its
    placement is decided. It is indexed because IOT-36 filters on it.
    """

    __tablename__ = "devices"

    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    location_id: Mapped[str | None] = mapped_column(
        ForeignKey("locations.id"), index=True, default=None
    )
    device_type: Mapped[str] = mapped_column(String(64), index=True)
    registered_at: Mapped[datetime] = mapped_column(UtcDateTime, default=_utcnow)

    location: Mapped["Location | None"] = relationship(back_populates="devices")

    def __repr__(self) -> str:
        return f"<Device {self.device_id!r} type={self.device_type!r}>"

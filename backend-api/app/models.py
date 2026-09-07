# A registry, not a record of events — storage-core's chain is the source of
# truth for what happened. Activity (last_seen, active) is derived, not stored.
#
# locations.id and devices.device_id are natural string keys matching the
# ledger's location_id/actor verbatim, so correlation needs no mapping table.
# Surrogate keys were rejected: the ledger is immutable, so old blocks could
# never be repointed and the mapping would live forever. Cost: renaming a
# location means re-flashing its devices.

from datetime import datetime, timezone

from sqlalchemy import ForeignKey, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db import Base, UtcDateTime


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


class User(Base):
    # No auth yet: password_hash is unused. Whoever adds auth must use a real
    # KDF (argon2/bcrypt) — never plaintext, never a bare SHA.
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    password_hash: Mapped[str | None] = mapped_column(String(255), default=None)
    role: Mapped[str] = mapped_column(String(32), default="operator")
    created_at: Mapped[datetime] = mapped_column(UtcDateTime, default=_utcnow)

    def __repr__(self) -> str:
        return f"<User {self.username!r} role={self.role!r}>"


class Location(Base):
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
    # App-wide, not per-location. Per-location overrides can be added when
    # something needs them.
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
    # No last_seen/active column on purpose — see the module comment.
    # location_id is nullable so a device can be registered before placement,
    # and indexed because IOT-36 filters on it.
    __tablename__ = "devices"

    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    location_id: Mapped[str | None] = mapped_column(
        ForeignKey("locations.id"), index=True, default=None
    )
    device_type: Mapped[str] = mapped_column(String(64), index=True)
    registered_at: Mapped[datetime] = mapped_column(UtcDateTime, default=_utcnow)

    location: Mapped["Location | None"] = relationship(back_populates="devices")
    sensors: Mapped[list["Sensor"]] = relationship(
        back_populates="device", cascade="all, delete-orphan"
    )

    def __repr__(self) -> str:
        return f"<Device {self.device_id!r} type={self.device_type!r}>"


class Sensor(Base):
    # A device is a board; sensors are what is attached to it (ADR 0010). One
    # device may carry several, they report at different rates and fail
    # independently, so each is its own row and its own stream of readings.
    #
    # Composite PK: sensor_id is unique within a device, not globally. Two boards
    # may both call their probe "temp-0", and a reading identifies itself by
    # exactly this pair — actor is the device, metadata.sensor_id the sensor.
    __tablename__ = "sensors"

    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.device_id", ondelete="CASCADE"), primary_key=True
    )
    sensor_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    sensor_type: Mapped[str] = mapped_column(String(32), index=True)
    # The unit this sensor is *expected* to report. The authoritative unit is the
    # one carried on each reading, so a swapped probe shows up in the data
    # instead of silently corrupting a chart.
    unit: Mapped[str] = mapped_column(String(32))
    registered_at: Mapped[datetime] = mapped_column(UtcDateTime, default=_utcnow)

    device: Mapped["Device"] = relationship(back_populates="sensors")

    def __repr__(self) -> str:
        return f"<Sensor {self.device_id!r}/{self.sensor_id!r} type={self.sensor_type!r}>"

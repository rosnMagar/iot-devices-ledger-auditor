"""IOT-34 — schema and constraint behaviour.

Each test gets its own in-memory SQLite database, so nothing leaks between
cases and no file is left behind.
"""

from datetime import datetime, timezone

import pytest
from sqlalchemy import create_engine, inspect
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from app.db import Base
from app.models import Device, Location, Setting, User


@pytest.fixture()
def session() -> Session:
    # StaticPool with a shared in-memory URL keeps every connection pointed at
    # the same database. Without it each connection gets its own blank one and
    # tables created on the first vanish from the second.
    engine = create_engine(
        "sqlite://",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    Base.metadata.create_all(bind=engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)
    with factory() as s:
        yield s
    engine.dispose()


def test_all_four_tables_are_created(session: Session) -> None:
    tables = set(inspect(session.get_bind()).get_table_names())
    assert {"users", "locations", "settings", "devices"} <= tables


def test_device_registers_against_a_location(session: Session) -> None:
    session.add(Location(id="loc-1", name="Warehouse A"))
    session.add(Device(device_id="esp32-01", location_id="loc-1", device_type="DHT22"))
    session.commit()

    device = session.get(Device, "esp32-01")
    assert device is not None
    assert device.location is not None
    assert device.location.name == "Warehouse A"
    assert device.location.devices == [device]


def test_device_ids_match_the_ledgers_string_keys(session: Session) -> None:
    # The whole point of the natural-key choice: these are the exact strings a
    # block carries in `actor` and `location_id`, so correlating a registry row
    # to ledger events needs no mapping table.
    session.add(Location(id="loc-1", name="Warehouse A"))
    session.add(Device(device_id="esp32-01", location_id="loc-1", device_type="DHT22"))
    session.commit()

    assert isinstance(session.get(Device, "esp32-01").device_id, str)
    assert isinstance(session.get(Location, "loc-1").id, str)


def test_device_location_is_optional(session: Session) -> None:
    # A device can be registered before anyone decides where it goes.
    session.add(Device(device_id="esp32-unplaced", device_type="DHT22"))
    session.commit()

    device = session.get(Device, "esp32-unplaced")
    assert device.location_id is None
    assert device.location is None


def test_device_cannot_reference_a_missing_location(session: Session) -> None:
    # SQLite disables foreign keys by default; app.db turns them on via PRAGMA.
    # If that listener regresses, this test is what notices.
    session.add(Device(device_id="esp32-01", location_id="nope", device_type="DHT22"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_duplicate_device_id_is_rejected(session: Session) -> None:
    session.add(Device(device_id="esp32-01", device_type="DHT22"))
    session.commit()
    session.add(Device(device_id="esp32-01", device_type="BME280"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_duplicate_username_is_rejected(session: Session) -> None:
    session.add(User(username="operator-1"))
    session.commit()
    session.add(User(username="operator-1"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_duplicate_setting_key_is_rejected(session: Session) -> None:
    session.add(Setting(key="alert_threshold", value="30"))
    session.commit()
    session.add(Setting(key="alert_threshold", value="40"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_user_defaults(session: Session) -> None:
    session.add(User(username="operator-1"))
    session.commit()

    user = session.query(User).one()
    assert user.role == "operator"
    # No auth yet — nothing writes this, and it must never hold a plaintext
    # password once something does.
    assert user.password_hash is None
    assert user.created_at is not None


def test_timestamps_are_timezone_aware_utc(session: Session) -> None:
    # Naive datetimes compare as local time and silently misorder rows once
    # anything crosses a timezone, so the default must carry an offset.
    session.add(Device(device_id="esp32-01", device_type="DHT22"))
    session.commit()

    registered_at = session.get(Device, "esp32-01").registered_at
    assert registered_at.tzinfo is not None
    assert registered_at.utcoffset() == timezone.utc.utcoffset(None)


def test_setting_updated_at_moves_on_change(session: Session) -> None:
    session.add(Setting(key="alert_threshold", value="30"))
    session.commit()
    first = session.query(Setting).one().updated_at

    session.query(Setting).one().value = "40"
    session.commit()
    second = session.query(Setting).one().updated_at

    assert second >= first


def test_no_active_or_last_seen_column_on_devices(session: Session) -> None:
    # Deliberate: activity is derived from the ledger in IOT-35, not stored.
    # A stored flag needs something to keep it true and drifts the moment that
    # fails. If a later ticket adds one, this failing test should be the prompt
    # to justify it rather than a nuisance to delete.
    columns = {c["name"] for c in inspect(session.get_bind()).get_columns("devices")}
    assert columns == {"device_id", "location_id", "device_type", "registered_at"}


def test_naive_datetime_written_is_read_back_as_utc(session: Session) -> None:
    # SQLite has no timezone type, so a value stored through a plain
    # DateTime(timezone=True) comes back naive while Postgres returns it aware.
    # UtcDateTime normalises both ends; this is the regression test for that.
    session.add(
        Device(
            device_id="esp32-naive",
            device_type="DHT22",
            # Suppression below is deliberate: a naive datetime is precisely
            # what is under test, so the lint rule is asking for the bug back.
            registered_at=datetime(2026, 1, 1, 12, 0, 0),  # noqa: DTZ001
        )
    )
    session.commit()
    session.expire_all()  # force a real round-trip through the database

    read_back = session.get(Device, "esp32-naive").registered_at
    assert read_back.tzinfo is not None
    assert read_back == datetime(2026, 1, 1, 12, 0, 0, tzinfo=timezone.utc)


def test_non_utc_input_is_normalised_to_utc(session: Session) -> None:
    from datetime import timedelta

    # 12:00 at UTC+02:00 is 10:00 UTC. Storing only UTC is what keeps ordering
    # correct across devices reporting from different offsets.
    plus_two = timezone(timedelta(hours=2))
    session.add(
        Device(
            device_id="esp32-offset",
            device_type="DHT22",
            registered_at=datetime(2026, 1, 1, 12, 0, 0, tzinfo=plus_two),
        )
    )
    session.commit()
    session.expire_all()

    read_back = session.get(Device, "esp32-offset").registered_at
    assert read_back == datetime(2026, 1, 1, 10, 0, 0, tzinfo=timezone.utc)

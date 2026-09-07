# IOT-71 — the sensors registry (ADR 0010): a device is a board, sensors are
# what is attached to it.

from datetime import timezone

import pytest
from sqlalchemy import create_engine, inspect
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from app.db import Base
from app.models import Device, Location, Sensor


@pytest.fixture()
def session() -> Session:
    engine = create_engine(
        "sqlite://", connect_args={"check_same_thread": False}, poolclass=StaticPool
    )
    Base.metadata.create_all(bind=engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)
    with factory() as s:
        s.add(Location(id="warehouse-a", name="Warehouse A"))
        s.add(Device(device_id="esp32-01", location_id="warehouse-a", device_type="ESP32"))
        s.add(Device(device_id="esp32-02", location_id="warehouse-a", device_type="ESP32"))
        s.commit()
        yield s
    engine.dispose()


def sensor(device_id: str, sensor_id: str, stype: str = "temperature", unit: str = "celsius"):
    return Sensor(device_id=device_id, sensor_id=sensor_id, sensor_type=stype, unit=unit)


def test_sensors_table_is_created(session: Session) -> None:
    assert "sensors" in set(inspect(session.get_bind()).get_table_names())


def test_a_device_can_carry_several_sensors(session: Session) -> None:
    # The whole reason ADR 0009 was superseded.
    session.add(sensor("esp32-01", "temp-0"))
    session.add(sensor("esp32-01", "hum-0", "humidity", "percent"))
    session.add(sensor("esp32-01", "cam-0", "camera", "stream"))
    session.commit()

    device = session.get(Device, "esp32-01")
    assert {s.sensor_id for s in device.sensors} == {"temp-0", "hum-0", "cam-0"}
    assert session.get(Sensor, ("esp32-01", "temp-0")).device.device_id == "esp32-01"


def test_two_thermometers_on_one_board(session: Session) -> None:
    # Impossible under the old flat payload; the point of the change.
    session.add(sensor("esp32-01", "temp-inlet"))
    session.add(sensor("esp32-01", "temp-outlet"))
    session.commit()

    assert len(session.get(Device, "esp32-01").sensors) == 2


def test_sensor_ids_are_unique_only_within_a_device(session: Session) -> None:
    # Two boards may both call their probe temp-0; a global PK would forbid it.
    session.add(sensor("esp32-01", "temp-0"))
    session.add(sensor("esp32-02", "temp-0"))
    session.commit()

    assert session.get(Sensor, ("esp32-01", "temp-0")) is not None
    assert session.get(Sensor, ("esp32-02", "temp-0")) is not None


def test_duplicate_sensor_id_within_one_device_is_rejected(session: Session) -> None:
    session.add(sensor("esp32-01", "temp-0"))
    session.commit()
    session.add(sensor("esp32-01", "temp-0", "humidity", "percent"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_a_sensor_cannot_reference_a_missing_device(session: Session) -> None:
    session.add(sensor("no-such-device", "temp-0"))
    with pytest.raises(IntegrityError):
        session.commit()


def test_deleting_a_device_removes_its_sensors(session: Session) -> None:
    # Orphaned sensor rows would show up as sensors of a device that is gone.
    session.add(sensor("esp32-01", "temp-0"))
    session.add(sensor("esp32-01", "hum-0", "humidity", "percent"))
    session.commit()
    assert session.query(Sensor).count() == 2

    session.delete(session.get(Device, "esp32-01"))
    session.commit()

    assert session.query(Sensor).count() == 0


def test_a_device_with_no_sensors_is_valid(session: Session) -> None:
    # A board awaiting commissioning. Must not be an error (ADR 0010).
    assert session.get(Device, "esp32-01").sensors == []


def test_registered_at_is_timezone_aware_utc(session: Session) -> None:
    session.add(sensor("esp32-01", "temp-0"))
    session.commit()

    registered_at = session.get(Sensor, ("esp32-01", "temp-0")).registered_at
    assert registered_at.tzinfo is not None
    assert registered_at.utcoffset() == timezone.utc.utcoffset(None)


def test_sensors_store_no_readings(session: Session) -> None:
    # Deliberate: the ledger is the source of truth for measurements. If a later
    # ticket adds a value column, this failing test is the prompt to justify it.
    columns = {c["name"] for c in inspect(session.get_bind()).get_columns("sensors")}
    assert columns == {"device_id", "sensor_id", "sensor_type", "unit", "registered_at"}

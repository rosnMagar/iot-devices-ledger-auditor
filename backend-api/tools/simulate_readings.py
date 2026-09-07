# Dev tool. Stands in for the ESP32 fleet (IOT-30...33, blocked on hardware) so
# the live feed and charts have data to show. Payload shape is ADR 0010.
#
# Runs inside the backend-api container, which is the only place that can reach
# both the devices DB and storage-core:
#
#   docker compose exec -T -e SIM_CONFIRM=1 backend-api \
#       python - < backend-api/tools/simulate_readings.py
#
# Every reading is appended to an immutable hash chain and can never be removed.
# That is why SIM_CONFIRM is mandatory and prod needs SIM_FORCE as well.

import json
import os
import random
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field

STORAGE_CORE_URL = os.environ.get("STORAGE_CORE_URL", "http://storage-core:8080")
DEVICE_COUNT = int(os.environ.get("SIM_DEVICES", "4"))
DURATION_SECONDS = float(os.environ.get("SIM_DURATION", "300"))
FAILURE_RATE = float(os.environ.get("SIM_FAILURE_RATE", "0.02"))
TICK_SECONDS = float(os.environ.get("SIM_TICK", "1"))
SEED = os.environ.get("SIM_SEED")

LOCATIONS = [("warehouse-a", "Warehouse A"), ("cold-store", "Cold Store")]


@dataclass(frozen=True)
class SensorSpec:
    # One kind of sensor: how it drifts, what it reports, and how often. Sensors
    # on the same board run on their own clocks (ADR 0010) — a camera does not
    # wait for a thermometer.
    sensor_type: str
    unit: str
    step: float
    bounds: tuple[float, float]
    interval: float
    vector: bool = False


SPECS = {
    "temperature": SensorSpec("temperature", "celsius", 0.4, (2.0, 32.0), 10.0),
    "humidity": SensorSpec("humidity", "percent", 1.2, (15.0, 85.0), 30.0),
    "pressure": SensorSpec("pressure", "hpa", 0.6, (950.0, 1050.0), 30.0),
    # Fast and vector-valued, so both edges of the contract get exercised.
    "accelerometer": SensorSpec("accelerometer", "m_s2", 0.8, (-20.0, 20.0), 2.0, vector=True),
    # No readings at all — see emit_camera_event and ADR 0011.
    "camera": SensorSpec("camera", "stream", 0.0, (0.0, 0.0), 60.0),
}


@dataclass
class SensorState:
    device_id: str
    sensor_id: str
    spec: SensorSpec
    value: float | list[float]
    baseline: float
    seq: int = 0
    due_at: float = 0.0


@dataclass
class DeviceState:
    device_id: str
    location_id: str
    device_type: str
    sensors: list[SensorState] = field(default_factory=list)


def _sensor_kinds(index: int) -> list[str]:
    # Deliberately uneven: a bare board, a two-sensor board, a board with two
    # thermometers, and a camera board. A fleet where every device is identical
    # would not exercise the panel.
    return [
        ["temperature", "humidity"],
        ["temperature", "humidity", "pressure"],
        ["temperature", "temperature", "accelerometer"],
        ["temperature", "camera"],
    ][index % 4]


def build_fleet(count: int) -> list[DeviceState]:
    fleet = []
    for i in range(count):
        location_id, _ = LOCATIONS[i % len(LOCATIONS)]
        device = DeviceState(
            device_id=f"esp32-{i + 1:02d}",
            location_id=location_id,
            device_type="ESP32",
        )
        # A cold store really is colder; a chart where every line overlaps
        # demonstrates nothing.
        cold = location_id == "cold-store"
        seen: dict[str, int] = {}
        for kind in _sensor_kinds(i):
            spec = SPECS[kind]
            n = seen.get(kind, 0)
            seen[kind] = n + 1
            # Two sensors of the same kind on one board need distinct ids — the
            # case the superseded flat payload could not express.
            sensor_id = f"{kind[:4]}-{n}"
            baseline = {
                "temperature": 4.0 if cold else 21.0,
                "humidity": 70.0 if cold else 45.0,
                "pressure": 1013.0,
                "accelerometer": 0.0,
                "camera": 0.0,
            }[kind]
            start: float | list[float] = (
                [0.0, 0.0, 9.81] if spec.vector else baseline + random.uniform(-1.0, 1.0)
            )
            device.sensors.append(
                SensorState(
                    device_id=device.device_id,
                    sensor_id=sensor_id,
                    spec=spec,
                    value=start,
                    baseline=baseline,
                )
            )
        fleet.append(device)
    return fleet


def _drift(value: float, baseline: float, step: float, bounds: tuple[float, float]) -> float:
    # Mean-reverting random walk: successive readings sit close together the way
    # real sensor data does. Independent samples would look like noise and would
    # hide a chart silently redrawing from scratch instead of appending.
    low, high = bounds
    moved = value + random.uniform(-step, step) + (baseline - value) * 0.05
    return round(min(max(moved, low), high), 2)


def advance(sensor: SensorState) -> SensorState:
    spec = sensor.spec
    if spec.vector:
        # Gravity sits on z; the other axes wander around zero.
        current = sensor.value if isinstance(sensor.value, list) else [0.0, 0.0, 9.81]
        sensor.value = [
            _drift(current[0], 0.0, spec.step, spec.bounds),
            _drift(current[1], 0.0, spec.step, spec.bounds),
            _drift(current[2], 9.81, spec.step, spec.bounds),
        ]
    elif spec.sensor_type != "camera":
        current = sensor.value if isinstance(sensor.value, float) else sensor.baseline
        sensor.value = _drift(current, sensor.baseline, spec.step, spec.bounds)
    sensor.seq += 1
    return sensor


def build_reading(sensor: SensorState, failed: bool) -> dict:
    # ADR 0010: a failed read is an explicit null, never a missing key, never 0.
    # One sensor failing says nothing about the others on the same board.
    return {
        "event_type": "SENSOR_READING",
        "location_id": "",  # filled in by the caller, which knows the device
        "actor": sensor.device_id,
        "description": "sensor read failed" if failed else "periodic reading",
        "metadata": {
            "sensor_id": sensor.sensor_id,
            "sensor_type": sensor.spec.sensor_type,
            "value": None if failed else sensor.value,
            "unit": sensor.spec.unit,
            "seq": sensor.seq,
        },
    }


def build_camera_event(sensor: SensorState, kind: str) -> dict:
    # ADR 0011: the ledger records events *about* the camera. No frame, still or
    # clip is ever written into a block.
    return {
        "event_type": "CAMERA_EVENT",
        "location_id": "",
        "actor": sensor.device_id,
        "description": kind.replace("_", " "),
        "metadata": {
            "sensor_id": sensor.sensor_id,
            "sensor_type": "camera",
            "event": kind,
            "seq": sensor.seq,
        },
    }


def looks_like_production() -> bool:
    # Heuristic, deliberately conservative: a CORS origin that is not localhost
    # means this backend is serving a real browser somewhere.
    origins = os.environ.get("CORS_ORIGINS", "")
    return bool(origins) and "localhost" not in origins


def check_guards() -> None:
    if os.environ.get("SIM_CONFIRM") != "1":
        sys.exit(
            "refusing to run: set SIM_CONFIRM=1.\n"
            "Every reading is appended to an immutable hash chain and cannot be "
            "removed afterwards."
        )
    if looks_like_production() and os.environ.get("SIM_FORCE") != "1":
        sys.exit(
            f"refusing to run: this looks like production "
            f"(CORS_ORIGINS={os.environ.get('CORS_ORIGINS')!r}).\n"
            "Simulated readings would be permanent in the production ledger. "
            "Set SIM_FORCE=1 only if that is genuinely what you want."
        )


def register(fleet: list[DeviceState]) -> None:
    # Imported here, not at module scope: importing app.db builds an engine, and
    # at import time that would create a stray SQLite file during tests.
    from app.db import SessionLocal, init_db
    from app.models import Device, Location, Sensor

    init_db()
    with SessionLocal() as session:
        for location_id, name in LOCATIONS:
            if session.get(Location, location_id) is None:
                session.add(Location(id=location_id, name=name))
        session.flush()
        for device in fleet:
            if session.get(Device, device.device_id) is None:
                session.add(
                    Device(
                        device_id=device.device_id,
                        location_id=device.location_id,
                        device_type=device.device_type,
                    )
                )
        session.flush()
        for device in fleet:
            for sensor in device.sensors:
                if session.get(Sensor, (device.device_id, sensor.sensor_id)) is None:
                    session.add(
                        Sensor(
                            device_id=device.device_id,
                            sensor_id=sensor.sensor_id,
                            sensor_type=sensor.spec.sensor_type,
                            unit=sensor.spec.unit,
                        )
                    )
        session.commit()
    total = sum(len(d.sensors) for d in fleet)
    print(f"registered {len(fleet)} devices with {total} sensors")


def post_event(payload: dict) -> int:
    request = urllib.request.Request(
        f"{STORAGE_CORE_URL}/events",
        data=json.dumps(payload).encode(),
        headers={"content-type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=5) as response:
        return response.status


def main() -> int:
    check_guards()
    if SEED is not None:
        random.seed(int(SEED))

    fleet = build_fleet(DEVICE_COUNT)
    register(fleet)

    print(
        f"posting to {STORAGE_CORE_URL} for {DURATION_SECONDS}s "
        f"— each sensor on its own interval, ctrl-c to stop"
    )
    started = time.monotonic()
    deadline = started + DURATION_SECONDS
    posted = failed_posts = 0

    while time.monotonic() < deadline:
        now = time.monotonic()
        for device in fleet:
            for sensor in device.sensors:
                if now < sensor.due_at:
                    continue
                sensor.due_at = now + sensor.spec.interval
                advance(sensor)

                if sensor.spec.sensor_type == "camera":
                    kind = "stream_online" if sensor.seq == 1 else "motion_detected"
                    payload = build_camera_event(sensor, kind)
                else:
                    # Each sensor fails on its own — one null must not null the
                    # others on the same board.
                    payload = build_reading(sensor, random.random() < FAILURE_RATE)

                payload["location_id"] = device.location_id
                try:
                    post_event(payload)
                    posted += 1
                except (urllib.error.URLError, OSError) as exc:
                    # Keep going: storage-core restarting mid-run should show up
                    # as a gap, the way a real fleet's outage would.
                    failed_posts += 1
                    print(f"  post failed for {sensor.sensor_id}: {exc}", file=sys.stderr)
        print(f"  {posted} events posted ({failed_posts} failed)", flush=True)
        time.sleep(TICK_SECONDS)

    print(f"done: {posted} events posted, {failed_posts} failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

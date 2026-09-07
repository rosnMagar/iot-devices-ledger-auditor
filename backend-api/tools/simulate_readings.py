# Dev tool. Stands in for the ESP32 fleet (IOT-30...33, blocked on hardware) so
# the live feed and charts have data to show. Payload shape is ADR 0009.
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
DEVICE_COUNT = int(os.environ.get("SIM_DEVICES", "5"))
INTERVAL_SECONDS = float(os.environ.get("SIM_INTERVAL", "10"))
DURATION_SECONDS = float(os.environ.get("SIM_DURATION", "300"))
FAILURE_RATE = float(os.environ.get("SIM_FAILURE_RATE", "0.02"))
SEED = os.environ.get("SIM_SEED")

# Plausible indoor ranges. A reading pinned to a bound is still a real reading,
# so these clamp rather than resample.
CELSIUS_RANGE = (2.0, 32.0)
HUMIDITY_RANGE = (15.0, 85.0)

LOCATIONS = [("warehouse-a", "Warehouse A"), ("cold-store", "Cold Store")]


@dataclass
class DeviceState:
    device_id: str
    location_id: str
    device_type: str
    celsius: float
    humidity_pct: float
    # Per-device baseline the walk is pulled back towards, so a long run does
    # not wander off into nonsense.
    celsius_baseline: float = 0.0
    humidity_baseline: float = 0.0
    seq: int = field(default=0)


def build_fleet(count: int) -> list[DeviceState]:
    fleet = []
    for i in range(count):
        location_id, _ = LOCATIONS[i % len(LOCATIONS)]
        # Cold store really is colder; a chart where every device sits on the
        # same line demonstrates nothing.
        base_c = 4.0 if location_id == "cold-store" else 21.0
        base_h = 70.0 if location_id == "cold-store" else 45.0
        celsius = base_c + random.uniform(-1.0, 1.0)
        humidity = base_h + random.uniform(-3.0, 3.0)
        fleet.append(
            DeviceState(
                device_id=f"esp32-{i + 1:02d}",
                location_id=location_id,
                device_type="DHT22" if i % 2 == 0 else "BME280",
                celsius=celsius,
                humidity_pct=humidity,
                celsius_baseline=base_c,
                humidity_baseline=base_h,
            )
        )
    return fleet


def _drift(value: float, baseline: float, step: float, pull: float, bounds) -> float:
    # Mean-reverting random walk: successive readings are close together the way
    # real sensor data is. Independent samples would look like noise and would
    # hide a chart that is silently redrawing from scratch.
    low, high = bounds
    moved = value + random.uniform(-step, step) + (baseline - value) * pull
    return round(min(max(moved, low), high), 2)


def advance(device: DeviceState) -> DeviceState:
    device.celsius = _drift(device.celsius, device.celsius_baseline, 0.4, 0.05, CELSIUS_RANGE)
    device.humidity_pct = _drift(
        device.humidity_pct, device.humidity_baseline, 1.2, 0.05, HUMIDITY_RANGE
    )
    device.seq += 1
    return device


def build_event(device: DeviceState, failed: bool) -> dict:
    # ADR 0009: a failed read is an explicit null, never a missing key, never 0.
    return {
        "event_type": "SENSOR_READING",
        "location_id": device.location_id,
        "actor": device.device_id,
        "description": "sensor read failed" if failed else "periodic reading",
        "metadata": {
            "celsius": None if failed else device.celsius,
            "humidity_pct": None if failed else device.humidity_pct,
            "seq": device.seq,
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
    from app.models import Device, Location

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
        session.commit()
    print(f"registered {len(fleet)} devices across {len(LOCATIONS)} locations")


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
        f"posting to {STORAGE_CORE_URL} every {INTERVAL_SECONDS}s "
        f"for {DURATION_SECONDS}s — ctrl-c to stop"
    )
    deadline = time.monotonic() + DURATION_SECONDS
    posted = failed_posts = 0

    while time.monotonic() < deadline:
        for device in fleet:
            advance(device)
            failed = random.random() < FAILURE_RATE
            try:
                post_event(build_event(device, failed))
                posted += 1
            except (urllib.error.URLError, OSError) as exc:
                # Keep going: storage-core restarting mid-run should not end the
                # simulation, it should show up as a gap the way a real one would.
                failed_posts += 1
                print(f"  post failed for {device.device_id}: {exc}", file=sys.stderr)
        print(f"  {posted} readings posted ({failed_posts} failed)", flush=True)
        time.sleep(INTERVAL_SECONDS)

    print(f"done: {posted} readings posted, {failed_posts} failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

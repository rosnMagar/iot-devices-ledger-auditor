# IOT-59 — the reading simulator: payload shape, plausible drift, and the guards
# that keep simulated data out of a ledger that cannot forget it.

import random

import pytest

from tools import simulate_readings as sim


@pytest.fixture(autouse=True)
def _deterministic():
    random.seed(1234)


def test_fleet_spreads_devices_across_locations_and_types() -> None:
    fleet = sim.build_fleet(5)

    assert [d.device_id for d in fleet] == [
        "esp32-01", "esp32-02", "esp32-03", "esp32-04", "esp32-05",
    ]
    assert {d.location_id for d in fleet} == {"warehouse-a", "cold-store"}
    assert {d.device_type for d in fleet} == {"DHT22", "BME280"}


def test_the_cold_store_is_actually_colder() -> None:
    # A chart where every device sits on one line demonstrates nothing.
    fleet = sim.build_fleet(4)
    cold = [d for d in fleet if d.location_id == "cold-store"]
    warm = [d for d in fleet if d.location_id == "warehouse-a"]

    assert max(d.celsius for d in cold) < min(d.celsius for d in warm)


def test_successive_readings_are_close_together() -> None:
    # Independent samples would look like noise, and would hide a chart that is
    # silently redrawing from scratch instead of appending.
    device = sim.build_fleet(1)[0]
    previous = device.celsius

    for _ in range(50):
        sim.advance(device)
        assert abs(device.celsius - previous) <= 1.0
        previous = device.celsius


def test_a_long_run_stays_within_plausible_bounds() -> None:
    fleet = sim.build_fleet(2)
    for _ in range(500):
        for device in fleet:
            sim.advance(device)
            assert sim.CELSIUS_RANGE[0] <= device.celsius <= sim.CELSIUS_RANGE[1]
            assert sim.HUMIDITY_RANGE[0] <= device.humidity_pct <= sim.HUMIDITY_RANGE[1]


def test_seq_increments_per_device() -> None:
    device = sim.build_fleet(1)[0]
    assert device.seq == 0
    sim.advance(device)
    sim.advance(device)
    assert device.seq == 2


def test_event_matches_the_adr_0009_shape() -> None:
    device = sim.advance(sim.build_fleet(1)[0])
    event = sim.build_event(device, failed=False)

    assert event["event_type"] == "SENSOR_READING"
    assert event["actor"] == device.device_id
    assert event["location_id"] == device.location_id
    assert set(event["metadata"]) == {"celsius", "humidity_pct", "seq"}
    assert isinstance(event["metadata"]["celsius"], float)


def test_a_failed_read_sends_explicit_nulls_not_missing_keys() -> None:
    # ADR 0009: omission cannot distinguish "sensor failed" from "block predates
    # the field", and the chain keeps both forever.
    device = sim.advance(sim.build_fleet(1)[0])
    event = sim.build_event(device, failed=True)

    assert "celsius" in event["metadata"]
    assert "humidity_pct" in event["metadata"]
    assert event["metadata"]["celsius"] is None
    assert event["metadata"]["humidity_pct"] is None
    # 0 would be indistinguishable from a freezing warehouse.
    assert event["metadata"]["celsius"] != 0
    # seq still advances, so a gap is visible rather than silent.
    assert event["metadata"]["seq"] == device.seq


class TestGuards:
    def test_refuses_without_confirmation(self, monkeypatch) -> None:
        monkeypatch.delenv("SIM_CONFIRM", raising=False)
        with pytest.raises(SystemExit) as exit_info:
            sim.check_guards()
        assert "SIM_CONFIRM" in str(exit_info.value)

    def test_runs_locally_once_confirmed(self, monkeypatch) -> None:
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://localhost:5173")
        sim.check_guards()  # must not raise

    def test_refuses_when_the_target_looks_like_production(self, monkeypatch) -> None:
        # Simulated readings are permanent in a hash chain; this is the guard
        # that stops a demo polluting the real ledger.
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://3.16.105.105")
        with pytest.raises(SystemExit) as exit_info:
            sim.check_guards()
        assert "production" in str(exit_info.value)

    def test_production_can_be_forced_deliberately(self, monkeypatch) -> None:
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("SIM_FORCE", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://3.16.105.105")
        sim.check_guards()  # must not raise

    def test_an_unset_cors_origin_is_not_treated_as_production(self, monkeypatch) -> None:
        monkeypatch.delenv("CORS_ORIGINS", raising=False)
        assert sim.looks_like_production() is False

# IOT-72 — the reading simulator: per-sensor payloads (ADR 0010), camera events
# that carry no media (ADR 0011), and the guards that keep simulated data out of
# a ledger that cannot forget it.

import random

import pytest

from tools import simulate_readings as sim


@pytest.fixture(autouse=True)
def _deterministic():
    random.seed(1234)


def sensors_of(device):
    return {s.sensor_id: s for s in device.sensors}


class TestFleet:
    def test_devices_carry_several_sensors(self) -> None:
        fleet = sim.build_fleet(4)
        assert all(len(d.sensors) >= 2 for d in fleet)
        assert {d.device_id for d in fleet} == {
            "esp32-01", "esp32-02", "esp32-03", "esp32-04",
        }

    def test_a_board_can_carry_two_of_the_same_sensor(self) -> None:
        # The case the superseded flat payload could not express.
        boards = [d for d in sim.build_fleet(4) if len(sensors_of(d)) > 0]
        doubled = [d for d in boards if sum(
            1 for s in d.sensors if s.spec.sensor_type == "temperature") == 2]

        assert doubled, "expected a board with two thermometers"
        ids = [s.sensor_id for s in doubled[0].sensors if s.spec.sensor_type == "temperature"]
        assert len(set(ids)) == 2, "two sensors of one kind need distinct ids"

    def test_the_fleet_is_not_uniform(self) -> None:
        # Identical devices would not exercise the panel.
        shapes = {
            tuple(sorted(s.spec.sensor_type for s in d.sensors))
            for d in sim.build_fleet(4)
        }
        assert len(shapes) > 1

    def test_sensor_types_run_on_different_intervals(self) -> None:
        intervals = {s.sensor_type: s.interval for s in sim.SPECS.values()}
        assert intervals["accelerometer"] < intervals["temperature"] < intervals["humidity"]

    def test_the_cold_store_is_actually_colder(self) -> None:
        fleet = sim.build_fleet(4)
        temps = {
            d.location_id: [s.value for s in d.sensors if s.spec.sensor_type == "temperature"]
            for d in fleet
        }
        assert max(temps["cold-store"]) < min(temps["warehouse-a"])


class TestDrift:
    def test_successive_readings_are_close_together(self) -> None:
        # Independent samples would look like noise, and would hide a chart that
        # is silently redrawing from scratch instead of appending.
        sensor = next(
            s for d in sim.build_fleet(1) for s in d.sensors
            if s.spec.sensor_type == "temperature"
        )
        previous = sensor.value
        for _ in range(50):
            sim.advance(sensor)
            assert abs(sensor.value - previous) <= 1.0
            previous = sensor.value

    def test_a_long_run_stays_within_bounds(self) -> None:
        fleet = sim.build_fleet(4)
        for _ in range(300):
            for device in fleet:
                for sensor in device.sensors:
                    sim.advance(sensor)
                    if sensor.spec.sensor_type == "camera":
                        continue
                    low, high = sensor.spec.bounds
                    values = sensor.value if isinstance(sensor.value, list) else [sensor.value]
                    assert all(low <= v <= high for v in values)

    def test_seq_increments_per_sensor_not_per_device(self) -> None:
        device = sim.build_fleet(1)[0]
        first, second = device.sensors[0], device.sensors[1]
        sim.advance(first)
        sim.advance(first)
        sim.advance(second)

        assert (first.seq, second.seq) == (2, 1)


class TestPayload:
    def temperature_sensor(self):
        return next(
            s for d in sim.build_fleet(1) for s in d.sensors
            if s.spec.sensor_type == "temperature"
        )

    def test_reading_matches_the_adr_0010_shape(self) -> None:
        sensor = sim.advance(self.temperature_sensor())
        event = sim.build_reading(sensor, failed=False)

        assert event["event_type"] == "SENSOR_READING"
        # actor stays the device, so IOT-35's last_seen derivation is unchanged.
        assert event["actor"] == sensor.device_id
        assert set(event["metadata"]) == {"sensor_id", "sensor_type", "value", "unit", "seq"}
        assert event["metadata"]["unit"] == "celsius"

    def test_a_failed_read_sends_an_explicit_null(self) -> None:
        sensor = sim.advance(self.temperature_sensor())
        event = sim.build_reading(sensor, failed=True)

        assert "value" in event["metadata"]
        assert event["metadata"]["value"] is None
        # 0 would be indistinguishable from a freezing warehouse.
        assert event["metadata"]["value"] != 0
        # seq still advances, so the gap is visible rather than silent.
        assert event["metadata"]["seq"] == sensor.seq

    def test_one_sensor_failing_does_not_null_the_others(self) -> None:
        # Independent failure is a large part of why ADR 0009 was superseded.
        device = sim.build_fleet(1)[0]
        good, bad = device.sensors[0], device.sensors[1]
        sim.advance(good)
        sim.advance(bad)

        assert sim.build_reading(good, failed=False)["metadata"]["value"] is not None
        assert sim.build_reading(bad, failed=True)["metadata"]["value"] is None

    def test_an_accelerometer_reports_a_three_axis_vector(self) -> None:
        sensor = next(
            s for d in sim.build_fleet(4) for s in d.sensors
            if s.spec.sensor_type == "accelerometer"
        )
        value = sim.build_reading(sim.advance(sensor), failed=False)["metadata"]["value"]

        assert isinstance(value, list) and len(value) == 3
        assert sensor.spec.unit == "m_s2"

    def test_a_failed_vector_read_is_a_single_null(self) -> None:
        sensor = next(
            s for d in sim.build_fleet(4) for s in d.sensors
            if s.spec.sensor_type == "accelerometer"
        )
        assert sim.build_reading(sim.advance(sensor), failed=True)["metadata"]["value"] is None


class TestCamera:
    def camera_sensor(self):
        return next(
            s for d in sim.build_fleet(4) for s in d.sensors
            if s.spec.sensor_type == "camera"
        )

    def test_a_camera_emits_events_not_readings(self) -> None:
        event = sim.build_camera_event(self.camera_sensor(), "stream_online")
        assert event["event_type"] == "CAMERA_EVENT"
        assert event["metadata"]["event"] == "stream_online"

    def test_a_camera_event_carries_no_media(self) -> None:
        # ADR 0011: no frame, still or clip ever enters a block. The chain is
        # undeletable, so footage in it could never be erased.
        event = sim.build_camera_event(self.camera_sensor(), "motion_detected")
        assert set(event["metadata"]) == {"sensor_id", "sensor_type", "event", "seq"}

        serialised = str(event)
        for forbidden in ("frame", "image", "jpeg", "base64", "data:"):
            assert forbidden not in serialised.lower()


class TestGuards:
    def test_refuses_without_confirmation(self, monkeypatch) -> None:
        monkeypatch.delenv("SIM_CONFIRM", raising=False)
        with pytest.raises(SystemExit) as exit_info:
            sim.check_guards()
        assert "SIM_CONFIRM" in str(exit_info.value)

    def test_runs_locally_once_confirmed(self, monkeypatch) -> None:
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://localhost:5173")
        sim.check_guards()

    def test_refuses_when_the_target_looks_like_production(self, monkeypatch) -> None:
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://3.16.105.105")
        with pytest.raises(SystemExit) as exit_info:
            sim.check_guards()
        assert "production" in str(exit_info.value)

    def test_production_can_be_forced_deliberately(self, monkeypatch) -> None:
        monkeypatch.setenv("SIM_CONFIRM", "1")
        monkeypatch.setenv("SIM_FORCE", "1")
        monkeypatch.setenv("CORS_ORIGINS", "http://3.16.105.105")
        sim.check_guards()

    def test_an_unset_cors_origin_is_not_treated_as_production(self, monkeypatch) -> None:
        monkeypatch.delenv("CORS_ORIGINS", raising=False)
        assert sim.looks_like_production() is False

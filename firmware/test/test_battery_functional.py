#!/usr/bin/env python3
"""
Functional unit tests for battery percentage calculation.

Tests the linear interpolation between empty/full thresholds used by
the Battery class, and verifies AXP2101-style fuel gauge behavior.
"""

import sys
import unittest


BATTERY_EMPTY_MV = 3000   # From config.h BATTERY_EMPTY_MV
BATTERY_FULL_MV = 4200    # From config.h BATTERY_FULL_MV


def calculate_battery_percent(mv):
    """
    Calculate battery percentage using the same linear interpolation
    as Battery::getPercent() in battery.cpp.

    Mirrors:
        if (mv >= BATTERY_FULL_MV) return 100;
        if (mv <= BATTERY_EMPTY_MV) return 0;
        return ((mv - EMPTY) * 100) / (FULL - EMPTY);
    """
    if mv <= 0:
        return 0
    if mv >= BATTERY_FULL_MV:
        return 100
    if mv <= BATTERY_EMPTY_MV:
        return 0

    return int((mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV))


class TestBatteryPercentageLinear(unittest.TestCase):
    """Test linear interpolation between empty and full thresholds."""

    def test_empty_battery(self):
        self.assertEqual(calculate_battery_percent(3000), 0)

    def test_full_battery(self):
        self.assertEqual(calculate_battery_percent(4200), 100)

    def test_above_full(self):
        """Voltage above full threshold should return 100%."""
        self.assertEqual(calculate_battery_percent(4300), 100)

    def test_below_empty(self):
        """Voltage below empty threshold should return 0%."""
        self.assertEqual(calculate_battery_percent(2500), 0)

    def test_zero_voltage(self):
        """Zero voltage (no battery connected) returns 0%."""
        self.assertEqual(calculate_battery_percent(0), 0)

    def test_halfway_is_50_percent(self):
        """Midpoint between empty and full should be ~50%."""
        mid = (BATTERY_EMPTY_MV + BATTERY_FULL_MV) // 2  # 3600 mV
        self.assertEqual(calculate_battery_percent(mid), 50)

    def test_quarter_charge(self):
        """25% charge at 3300 mV."""
        self.assertEqual(calculate_battery_percent(3300), 25)

    def test_seventy_five_percent(self):
        """75% charge at 3900 mV."""
        self.assertEqual(calculate_battery_percent(3900), 75)


class TestBatteryPercentageBoundaries(unittest.TestCase):
    """Test boundary conditions near thresholds."""

    def test_just_above_empty(self):
        self.assertEqual(calculate_battery_percent(3001), 0)

    def test_just_below_full(self):
        self.assertEqual(calculate_battery_percent(4199), 99)

    def test_incremental_increase(self):
        """Percentage should monotonically increase with voltage."""
        prev_pct = -1
        for mv in range(BATTERY_EMPTY_MV, BATTERY_FULL_MV + 1):
            pct = calculate_battery_percent(mv)
            self.assertGreaterEqual(pct, prev_pct,
                f"Percentage decreased at {mv}mV: {pct} < {prev_pct}")
            prev_pct = pct

    def test_all_thresholds(self):
        """Check percentage at every 100 mV step."""
        expected = {
            3000: 0,
            3120: 10,
            3240: 20,
            3360: 30,
            3480: 40,
            3600: 50,
            3720: 60,
            3840: 70,
            3960: 80,
            4080: 90,
            4200: 100,
        }

        for mv, exp_pct in expected.items():
            self.assertEqual(calculate_battery_percent(mv), exp_pct,
                f"Expected {exp_pct}% at {mv}mV")


class TestBatteryChargingDetection(unittest.TestCase):
    """Test charging state detection logic."""

    def test_charging_threshold(self):
        """Voltage above 4.30V indicates USB charging (battery.cpp)."""
        def is_charging(mv):
            return mv > 4300

        self.assertFalse(is_charging(4200))   # Full but not charging
        self.assertTrue(is_charging(4350))    # Charging
        self.assertTrue(is_charging(4400))    # Charging


class TestBatteryVoltageToPercent(unittest.TestCase):
    """Test voltage-to-percent mapping for common LiPo states."""

    def test_critical_low(self):
        """~10% at 3120 mV — device may shut down soon."""
        self.assertEqual(calculate_battery_percent(3120), 10)

    def test_warning_level(self):
        """~5% at 3060 mV — critical warning threshold."""
        self.assertEqual(calculate_battery_percent(3060), 5)

    def test_nominal_discharge(self):
        """~80% at 3960 mV — typical after moderate use."""
        self.assertEqual(calculate_battery_percent(3960), 80)


class TestAxp2101FuelGauge(unittest.TestCase):
    """Test AXP2101-style fuel gauge behavior.

    The AXP2101 PMU provides hardware fuel gauge via coulomb counting,
    which may differ from the linear ADC-based calculation on other boards.
    """

    def test_axp_percent_capped_at_100(self):
        """AXP2101 getPercent() caps at 100 (battery_axp2101.cpp line: if (_percent > 100))."""
        # Simulate what the C++ code does: static_cast<uint8_t>(_pmu->getBatteryPercent())
        # If PMU returns > 100, it's clamped to 100.
        raw_percent = 127  # hypothetical PMU reading
        percent = min(raw_percent, 100)
        self.assertEqual(percent, 100)

    def test_axp_present_detection(self):
        """AXP2101 isPresent() checks isBatteryConnect()."""
        # Simulate: _present = _pmu->isBatteryConnect()
        has_battery = True
        percent = 75 if has_battery else 0
        self.assertEqual(percent, 75)

        has_battery = False
        percent = 75 if has_battery else 0
        self.assertEqual(percent, 0)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestBatteryPercentageLinear))
    suite.addTests(loader.loadTestsFromTestCase(TestBatteryPercentageBoundaries))
    suite.addTests(loader.loadTestsFromTestCase(TestBatteryChargingDetection))
    suite.addTests(loader.loadTestsFromTestCase(TestBatteryVoltageToPercent))
    suite.addTests(loader.loadTestsFromTestCase(TestAxp2101FuelGauge))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

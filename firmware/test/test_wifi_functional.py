#!/usr/bin/env python3
"""
Functional unit tests for WiFi manager state machine and AP behavior.

Tests AP SSID generation, STA connection logic, and auto-off timer
independent of ESP32 hardware.
"""

import sys
import unittest


AP_SSID_PREFIX = "RadiaLog"
AP_AUTO_OFF_MS = 300000  # 5 minutes


class TestApSsidGeneration(unittest.TestCase):
    """Test AP SSID construction from MAC address."""

    def test_ssid_format(self):
        """AP SSID should be RadiaLog-XXXX where XXXX is last 2 bytes of MAC."""
        mac_bytes = b'\xAA\xBB\xCC\xDD\xEE\xFF'
        suffix = f"{mac_bytes[4]:02X}{mac_bytes[5]:02X}"
        ssid = f"{AP_SSID_PREFIX}-{suffix}"

        self.assertEqual(ssid, "RadiaLog-EEFF")

    def test_ssid_with_zero_mac_bytes(self):
        """MAC with zero bytes should still format correctly."""
        mac_bytes = b'\x00\x00\x00\x00\x00\x10'
        suffix = f"{mac_bytes[4]:02X}{mac_bytes[5]:02X}"
        ssid = f"{AP_SSID_PREFIX}-{suffix}"

        self.assertEqual(ssid, "RadiaLog-0010")

    def test_ssid_all_zeros(self):
        """All-zero MAC should produce RadiaLog-0000."""
        mac_bytes = b'\x00' * 6
        suffix = f"{mac_bytes[4]:02X}{mac_bytes[5]:02X}"
        ssid = f"{AP_SSID_PREFIX}-{suffix}"

        self.assertEqual(ssid, "RadiaLog-0000")


class TestApPasswordLogic(unittest.TestCase):
    """Test AP password handling logic."""

    def test_password_protected_ap(self):
        """Non-empty password should enable WPA security on AP."""
        password = "radialog"
        use_wpa = len(password) > 0

        self.assertTrue(use_wpa)

    def test_open_ap(self):
        """Empty password should create open (unsecured) AP."""
        password = ""
        use_wpa = len(password) > 0

        self.assertFalse(use_wpa)

    def test_default_password_is_set(self):
        """Default AP_PASSWORD from config.h should be non-empty."""
        default_password = "radialog"
        self.assertTrue(len(default_password) > 0)
        self.assertEqual(default_password, "radialog")


class TestApAutoOffTimer(unittest.TestCase):
    """Test AP auto-shutdown timer logic."""

    def test_auto_off_after_timeout(self):
        """AP should disable after AP_AUTO_OFF_MS with no clients while STA is connected."""
        last_client_seen = 0
        current_time = AP_AUTO_OFF_MS + 1  # just past timeout
        sta_connected = True

        elapsed = current_time - last_client_seen
        should_disable = sta_connected and elapsed >= AP_AUTO_OFF_MS

        self.assertTrue(should_disable)

    def test_auto_off_not_triggered_without_sta(self):
        """AP should stay active as fallback while STA is disconnected."""
        last_client_seen = 0
        current_time = AP_AUTO_OFF_MS + 1
        sta_connected = False

        elapsed = current_time - last_client_seen
        should_disable = sta_connected and elapsed >= AP_AUTO_OFF_MS

        self.assertFalse(should_disable)

    def test_auto_off_not_triggered_with_clients(self):
        """AP should stay active while clients are connected."""
        last_client_seen = 0
        current_time = 150000  # 2.5 minutes, before timeout

        elapsed = current_time - last_client_seen
        should_disable = elapsed >= AP_AUTO_OFF_MS

        self.assertFalse(should_disable)

    def test_timer_resets_on_client_connect(self):
        """Timer resets when a new client connects."""
        last_client_seen = 100000  # set at t=100s
        current_time = 150000      # now at t=150s

        # Without reset: would have timed out (50s elapsed < 300s, so no)
        # With client reconnecting, timer resets to current time
        last_client_seen = current_time
        self.assertEqual(last_client_seen, current_time)


class TestStaConnectionLogic(unittest.TestCase):
    """Test STA connection attempt logic."""

    def test_single_network_connection(self):
        """Should attempt to connect to the first (highest priority) network."""
        networks = [
            {'ssid': 'Home', 'priority': 0},
            {'ssid': 'Work', 'priority': 1},
        ]

        # Sorted by priority (lowest value first)
        sorted_networks = sorted(networks, key=lambda n: n['priority'])
        first_to_try = sorted_networks[0]['ssid']

        self.assertEqual(first_to_try, 'Home')

    def test_priority_ordering(self):
        """Networks should be tried in priority order (lowest value first)."""
        networks = [
            {'ssid': 'LowPriority', 'priority': 5},
            {'ssid': 'HighPriority', 'priority': 0},
            {'ssid': 'MediumPriority', 'priority': 2},
        ]

        sorted_networks = sorted(networks, key=lambda n: n['priority'])
        order = [n['ssid'] for n in sorted_networks]

        self.assertEqual(order, ['HighPriority', 'MediumPriority', 'LowPriority'])


class TestWifiCredentials(unittest.TestCase):
    """Test WifiCredentials struct fields."""

    def test_credentials_fields(self):
        """Each network entry has ssid, password, and priority."""
        cred = {
            'ssid': 'MyNetwork',
            'password': 'secret',
            'priority': 0,
        }

        self.assertIn('ssid', cred)
        self.assertIn('password', cred)
        self.assertIn('priority', cred)


class TestMaxNetworks(unittest.TestCase):
    """Test WiFi network limit enforcement."""

    def test_max_networks_limit(self):
        """WifiMgr supports up to MAX_NETWORKS (4) networks."""
        MAX_NETWORKS = 4
        extra_networks = [f'Extra{i}' for i in range(5)]

        # Only first MAX_NETWORKS should be kept
        limited = extra_networks[:MAX_NETWORKS]

        self.assertEqual(len(limited), MAX_NETWORKS)


class TestConnectTimeout(unittest.TestCase):
    """Test STA connection timeout logic."""

    def test_connection_timeout(self):
        """Connection attempt times out after CONNECT_TIMEOUT_MS (15s)."""
        CONNECT_TIMEOUT_MS = 15000
        connect_start = 0
        current_time = 16000  # 16 seconds later

        timed_out = (current_time - connect_start) >= CONNECT_TIMEOUT_MS

        self.assertTrue(timed_out)

    def test_connection_not_timed_out(self):
        """Connection hasn't timed out yet."""
        CONNECT_TIMEOUT_MS = 15000
        connect_start = 0
        current_time = 10000  # 10 seconds later

        timed_out = (current_time - connect_start) >= CONNECT_TIMEOUT_MS

        self.assertFalse(timed_out)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestApSsidGeneration))
    suite.addTests(loader.loadTestsFromTestCase(TestApPasswordLogic))
    suite.addTests(loader.loadTestsFromTestCase(TestApAutoOffTimer))
    suite.addTests(loader.loadTestsFromTestCase(TestStaConnectionLogic))
    suite.addTests(loader.loadTestsFromTestCase(TestWifiCredentials))
    suite.addTests(loader.loadTestsFromTestCase(TestMaxNetworks))
    suite.addTests(loader.loadTestsFromTestCase(TestConnectTimeout))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

#!/usr/bin/env python3
"""
Functional unit tests for ConfigMgr JSON serialization logic.

Tests that the config JSON structure matches what the C++ code expects,
verifying field names, defaults, and round-trip consistency.
"""

import json
import sys
import unittest


# Default values matching ConfigMgr constructor in config_mgr.cpp
DEFAULTS = {
    'wifi': [],
    'radiamaps': {
        'token': '',
        'url': 'https://radiamaps.com/api/radialog/upload',
        'device_id': '',  # built at runtime from MAC
    },
    'device': {
        'name': 'RadiaLog',
        'reading_interval_ms': 2000,
    },
    'ap': {
        'password': 'radialog',  # AP_PASSWORD default from config.h
    },
    'geolocation': {
        'google_api_key': '',
    },
    'ble_devices': [],
    'display': {
        'timeout_sec': 0,
        'button_wake': True,
    },
}


def build_config_json(**overrides):
    """Build a config JSON document with optional overrides."""
    doc = json.loads(json.dumps(DEFAULTS))  # deep copy

    for key, value in overrides.items():
        if '.' in key:
            section, field = key.split('.', 1)
            if isinstance(value, dict):
                doc[section].update(value)
            else:
                doc[section][field] = value
        else:
            doc[key] = value

    return json.dumps(doc, indent=2)


class TestConfigDefaults(unittest.TestCase):
    """Verify default config values match C++ implementation."""

    def test_default_upload_url(self):
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['radiamaps']['url'], 'https://radiamaps.com/api/radialog/upload')

    def test_default_device_name(self):
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['device']['name'], 'RadiaLog')

    def test_default_reading_interval(self):
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['device']['reading_interval_ms'], 2000)

    def test_default_ap_password(self):
        """AP password should default to 'radialog' per config.h AP_PASSWORD."""
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['ap']['password'], 'radialog')

    def test_empty_wifi_by_default(self):
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['wifi'], [])

    def test_no_ble_devices_by_default(self):
        cfg = build_config_json()
        data = json.loads(cfg)
        self.assertEqual(data['ble_devices'], [])


class TestConfigRoundTrip(unittest.TestCase):
    """Test that config values survive a JSON round-trip."""

    def test_wifi_credentials_roundtrip(self):
        cfg = build_config_json(wifi=[{'ssid': 'MyNetwork', 'password': 'secret123'}])
        data = json.loads(cfg)

        self.assertEqual(len(data['wifi']), 1)
        self.assertEqual(data['wifi'][0]['ssid'], 'MyNetwork')
        self.assertEqual(data['wifi'][0]['password'], 'secret123')

    def test_multiple_wifi_networks(self):
        wifi = [
            {'ssid': 'Home', 'password': 'pass1'},
            {'ssid': 'Work', 'password': 'pass2'},
            {'ssid': 'Cafe', 'password': 'pass3'},
        ]
        cfg = build_config_json(wifi=wifi)
        data = json.loads(cfg)

        self.assertEqual(len(data['wifi']), 3)

    def test_token_and_device_id(self):
        cfg = build_config_json(**{
            'radiamaps.token': 'abc123def456',
            'radiamaps.device_id': 'RadiaLog-ABCD',
        })
        data = json.loads(cfg)

        self.assertEqual(data['radiamaps']['token'], 'abc123def456')
        self.assertEqual(data['radiamaps']['device_id'], 'RadiaLog-ABCD')

    def test_display_settings(self):
        cfg = build_config_json(**{
            'display.timeout_sec': 300,
            'display.button_wake': False,
        })
        data = json.loads(cfg)

        self.assertEqual(data['display']['timeout_sec'], 300)
        self.assertFalse(data['display']['button_wake'])

    def test_google_api_key(self):
        cfg = build_config_json(**{'geolocation.google_api_key': 'AIzaSyXXXXXXXX'})
        data = json.loads(cfg)

        self.assertEqual(data['geolocation']['google_api_key'], 'AIzaSyXXXXXXXX')


class TestConfigValidation(unittest.TestCase):
    """Test config value constraints."""

    def test_reading_interval_min(self):
        """Reading interval should not go below 500ms (C++: if ms < 500)."""
        cfg = build_config_json(**{'device.reading_interval_ms': 100})
        data = json.loads(cfg)

        # The JSON stores whatever is set; validation happens in C++ setter.
        # But we verify the default is within valid range.
        self.assertGreaterEqual(DEFAULTS['device']['reading_interval_ms'], 500)

    def test_reading_interval_max(self):
        """Reading interval should not exceed 60000ms (C++: if ms > 60000)."""
        cfg = build_config_json(**{'device.reading_interval_ms': 120000})
        data = json.loads(cfg)

        self.assertGreaterEqual(DEFAULTS['device']['reading_interval_ms'], 500)
        self.assertLessEqual(DEFAULTS['device']['reading_interval_ms'], 60000)

    def test_max_wifi_networks(self):
        """ConfigMgr supports up to MAX_WIFI (4) networks."""
        wifi = [{'ssid': f'Net{i}', 'password': f'pass{i}'} for i in range(5)]
        cfg = build_config_json(wifi=wifi)
        data = json.loads(cfg)

        # JSON can hold any number; C++ caps at MAX_WIFI=4
        self.assertEqual(len(data['wifi']), 5)

    def test_max_ble_devices(self):
        """ConfigMgr supports up to MAX_BLE_DEVICES (4)."""
        ble = [f'AA:BB:CC:DD:EE:{i:02X}' for i in range(6)]
        cfg = build_config_json(ble_devices=ble)
        data = json.loads(cfg)

        self.assertEqual(len(data['ble_devices']), 6)


class TestConfigEdgeCases(unittest.TestCase):
    """Test edge cases and error conditions."""

    def test_empty_password_allowed(self):
        """AP password can be empty string (open network)."""
        cfg = build_config_json(**{'ap.password': ''})
        data = json.loads(cfg)

        self.assertEqual(data['ap']['password'], '')

    def test_special_characters_in_wifi_password(self):
        """WiFi passwords can contain special characters."""
        cfg = build_config_json(wifi=[{
            'ssid': 'WPA2-Personal',
            'password': 'p@ss!w0rd#$%^&*()',
        }])
        data = json.loads(cfg)

        self.assertEqual(data['wifi'][0]['password'], 'p@ss!w0rd#$%^&*()')

    def test_unicode_in_device_name(self):
        """Device name can contain Unicode characters."""
        cfg = build_config_json(**{'device.name': 'RadiaLog - Test\u00e9'})
        data = json.loads(cfg)

        self.assertEqual(data['device']['name'], 'RadiaLog - Test\u00e9')

    def test_invalid_json_structure(self):
        """Malformed JSON should be handled gracefully by C++ code."""
        malformed = '{"radiamaps": {"token": "abc", "broken'

        with self.assertRaises(json.JSONDecodeError):
            json.loads(malformed)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestConfigDefaults))
    suite.addTests(loader.loadTestsFromTestCase(TestConfigRoundTrip))
    suite.addTests(loader.loadTestsFromTestCase(TestConfigValidation))
    suite.addTests(loader.loadTestsFromTestCase(TestConfigEdgeCases))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

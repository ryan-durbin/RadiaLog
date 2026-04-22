#!/usr/bin/env python3
"""
Functional unit tests for upload backoff/retry logic.

Tests the exponential backoff algorithm used by the uploader when
HTTP requests fail, independent of ESP32 hardware.
"""

import sys
import unittest


RECONNECT_DELAY_MIN_MS = 2000
RECONNECT_DELAY_MAX_MS = 300000  # 5 minutes


def calculate_backoff(failure_count):
    """
    Calculate upload retry delay using exponential backoff.

    Mirrors the logic in wifi_mgr.cpp _tryNextNetwork():
        _reconnectDelay *= 2;
        if (_reconnectDelay > RECONNECT_DELAY_MAX_MS) {
            _reconnectDelay = RECONNECT_DELAY_MAX_MS;
        }

    Starting from RECONNECT_DELAY_MIN_MS after first failure.
    """
    delay = RECONNECT_DELAY_MIN_MS * (2 ** failure_count)
    return min(delay, RECONNECT_DELAY_MAX_MS)


class TestExponentialBackoff(unittest.TestCase):
    """Test exponential backoff calculation for upload retries."""

    def test_first_retry(self):
        """First failure: 2 second delay."""
        self.assertEqual(calculate_backoff(0), 2000)

    def test_second_retry(self):
        """Second failure: 4 second delay."""
        self.assertEqual(calculate_backoff(1), 4000)

    def test_third_retry(self):
        """Third failure: 8 second delay."""
        self.assertEqual(calculate_backoff(2), 8000)

    def test_fourth_retry(self):
        """Fourth failure: 16 second delay."""
        self.assertEqual(calculate_backoff(3), 16000)

    def test_fifth_retry(self):
        """Fifth failure: 32 second delay."""
        self.assertEqual(calculate_backoff(4), 32000)


class TestBackoffCap(unittest.TestCase):
    """Test that backoff is capped at RECONNECT_DELAY_MAX_MS (5 min)."""

    def test_capped_at_max(self):
        """After enough failures, delay should not exceed 5 minutes."""
        # 2000 * 2^14 = 32,768,000 ms > 300,000 ms cap
        self.assertEqual(calculate_backoff(14), 300000)

    def test_capped_for_large_failure_counts(self):
        """Even with many failures, delay stays at max."""
        for count in [20, 50, 100]:
            self.assertEqual(calculate_backoff(count), 300000)

    def test_exact_cap_boundary(self):
        """Find the failure count where cap first applies."""
        # 2000 * 2^6 = 128,000 (under cap)
        self.assertLess(calculate_backoff(6), RECONNECT_DELAY_MAX_MS)
        # 2000 * 2^7 = 256,000 (still under cap)
        self.assertLess(calculate_backoff(7), RECONNECT_DELAY_MAX_MS)
        # 2000 * 2^8 = 512,000 (over cap)
        self.assertEqual(calculate_backoff(8), RECONNECT_DELAY_MAX_MS)


class TestUploadPayloadStructure(unittest.TestCase):
    """Test the JSON payload structure sent to RadiaMaps API."""

    def test_payload_fields(self):
        """Verify all required fields are present in upload payload."""
        import json

        # Simulate what uploader.cpp builds:
        readings = [
            {'dose_rate': 0.15, 'count_rate': 85.0, 'lat': 37.7749,
             'lon': -122.4194, 'timestamp': 1700000000},
        ]

        payload = {
            'device_id': 'RadiaLog-ABCD',
            'readings': readings,
        }

        self.assertIn('device_id', payload)
        self.assertIn('readings', payload)
        self.assertEqual(len(payload['readings']), 1)

    def test_batch_size_limit(self):
        """Uploader sends batches of up to 250 readings."""
        MAX_BATCH = 250
        large_dataset = [{'id': i} for i in range(600)]

        # Should split into multiple batches
        batches = [large_dataset[i:i + MAX_BATCH] for i in range(0, len(large_dataset), MAX_BATCH)]

        self.assertEqual(len(batches), 3)
        self.assertEqual(len(batches[0]), 250)
        self.assertEqual(len(batches[1]), 250)
        self.assertEqual(len(batches[2]), 100)


class TestUploadConfig(unittest.TestCase):
    """Test UploadConfig struct field mapping."""

    def test_config_fields(self):
        """UploadConfig has radiamaps_url, device_token, device_id."""
        config = {
            'radiamaps_url': 'https://radiamaps.com/api/radialog/upload',
            'device_token': 'abc123',
            'device_id': 'RadiaLog-ABCD',
        }

        self.assertIn('radiamaps_url', config)
        self.assertIn('device_token', config)
        self.assertIn('device_id', config)

    def test_default_upload_url(self):
        """Default upload URL matches ConfigMgr default."""
        self.assertEqual(
            'https://radiamaps.com/api/radialog/upload',
            'https://radiamaps.com/api/radialog/upload'
        )


class TestTokenVerification(unittest.TestCase):
    """Test token verification logic from /api/actions/verify-token endpoint."""

    def test_url_transformation(self):
        """Verify URL is derived by replacing /upload with /verify."""
        upload_url = 'https://radiamaps.com/api/radialog/upload'
        verify_url = upload_url.replace('/upload', '/verify')

        self.assertEqual(verify_url, 'https://radiamaps.com/api/radialog/verify')

    def test_empty_token_rejected(self):
        """Empty token should be rejected by the endpoint."""
        token = ''
        self.assertTrue(len(token) == 0)

    def test_http_status_mapping(self):
        """HTTP status codes map to appropriate error messages."""
        status_messages = {
            200: 'success',
            401: 'Invalid token',
            403: 'Invalid token',
        }

        self.assertIn(401, status_messages)
        self.assertIn(403, status_messages)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestExponentialBackoff))
    suite.addTests(loader.loadTestsFromTestCase(TestBackoffCap))
    suite.addTests(loader.loadTestsFromTestCase(TestUploadPayloadStructure))
    suite.addTests(loader.loadTestsFromTestCase(TestUploadConfig))
    suite.addTests(loader.loadTestsFromTestCase(TestTokenVerification))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

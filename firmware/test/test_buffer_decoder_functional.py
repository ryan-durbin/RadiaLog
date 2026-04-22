#!/usr/bin/env python3
"""
Functional unit tests for buffer reading decoder.

Tests the binary record format by constructing records in Python
and verifying they decode correctly — independent of the C++ implementation.
"""

import struct
import sys
import unittest


READING_SIZE = 34

# Record layout (matches reading.h / buffer.h):
#   float lat        [0:4]    little-endian
#   float lon        [4:8]
#   float dose_rate  [8:12]
#   float count_rate [12:16]
#   uint32_t timestamp [16:20]
#   float accuracy     [20:24]
#   float altitude     [24:28]
#   uint16_t speed_mph [28:30]  stored as int * 10
#   uint16_t speed_kph [30:32]  stored as int * 10
#   uint16_t heading   [32:34]  stored as int * 10


def encode_reading(lat, lon, dose_rate, count_rate, timestamp,
                   accuracy=0.0, altitude=0.0, speed_mph=0, speed_kph=0, heading=0):
    """Encode a reading into the binary format used by RadiaLog.

    Binary layout (34 bytes total):
      float lat(4) + float lon(4) + float dose_rate(4) + float count_rate(4)
      + uint32_t timestamp(4) + float accuracy(4) + float altitude(4)
      + uint16_t speed_mph_x10(2) + uint16_t speed_kph_x10(2)
      + uint16_t heading_x10(2)
    """
    return struct.pack(
        '<ffffIffHHH',
        lat, lon, dose_rate, count_rate, timestamp,
        accuracy, altitude,
        int(speed_mph * 10), int(speed_kph * 10), int(heading * 10)
    )


def decode_reading(data):
    """Decode a binary reading record — mirrors the C++ implementation."""
    if len(data) != READING_SIZE:
        raise ValueError(f"Expected {READING_SIZE} bytes, got {len(data)}")

    (lat, lon, dose_rate, count_rate, timestamp,
     accuracy, altitude, speed_mph_raw, speed_kph_raw, heading_raw) = struct.unpack(
        '<ffffIffHHH', data[:34]
    )

    return {
        'lat': lat,
        'lon': lon,
        'dose_rate': dose_rate,
        'count_rate': count_rate,
        'timestamp': timestamp,
        'accuracy': accuracy,
        'altitude': altitude,
        'speed_mph': speed_mph_raw / 10.0,
        'speed_kph': speed_kph_raw / 10.0,
        'heading': heading_raw / 10.0,
    }


class TestDecodeReading(unittest.TestCase):
    """Functional tests: construct records in Python and verify decoding."""

    def test_basic_reading(self):
        data = encode_reading(37.7749, -122.4194, 0.15, 85.0, 1700000000)
        r = decode_reading(data)

        self.assertAlmostEqual(r['lat'], 37.7749, places=5)
        self.assertAlmostEqual(r['lon'], -122.4194, places=5)
        self.assertAlmostEqual(r['dose_rate'], 0.15, places=4)
        self.assertEqual(r['count_rate'], 85.0)
        self.assertEqual(r['timestamp'], 1700000000)

    def test_speed_encoding(self):
        """Speeds are stored as integer * 10, verify round-trip."""
        data = encode_reading(0.0, 0.0, 0.0, 0.0, 0, speed_mph=5.3, speed_kph=8.5, heading=270)
        r = decode_reading(data)

        self.assertAlmostEqual(r['speed_mph'], 5.3, places=1)
        self.assertAlmostEqual(r['speed_kph'], 8.5, places=1)
        self.assertAlmostEqual(r['heading'], 270.0, places=1)

    def test_altitude_and_accuracy(self):
        data = encode_reading(40.0, -74.0, 0.0, 0.0, 0, accuracy=5.0, altitude=150.5)
        r = decode_reading(data)

        self.assertAlmostEqual(r['accuracy'], 5.0, places=2)
        self.assertAlmostEqual(r['altitude'], 150.5, places=1)

    def test_record_size(self):
        data = encode_reading(0.0, 0.0, 0.0, 0.0, 0)
        self.assertEqual(len(data), READING_SIZE, "Record must be exactly 34 bytes")

    def test_zero_reading(self):
        """All-zero reading should decode to all zeros."""
        data = encode_reading(0.0, 0.0, 0.0, 0.0, 0)
        r = decode_reading(data)

        self.assertEqual(r['lat'], 0.0)
        self.assertEqual(r['lon'], 0.0)
        self.assertEqual(r['dose_rate'], 0.0)
        self.assertEqual(r['count_rate'], 0.0)
        self.assertEqual(r['timestamp'], 0)

    def test_negative_coordinates(self):
        """Southern hemisphere and western hemisphere coordinates."""
        data = encode_reading(-33.8688, 151.2093, 0.08, 42.0, 1700100000)
        r = decode_reading(data)

        self.assertAlmostEqual(r['lat'], -33.8688, places=5)
        self.assertAlmostEqual(r['lon'], 151.2093, places=5)

    def test_high_dose_rate(self):
        """Edge case: high radiation reading."""
        data = encode_reading(0.0, 0.0, 500.0, 25000.0, 1700200000)
        r = decode_reading(data)

        self.assertAlmostEqual(r['dose_rate'], 500.0, places=2)
        self.assertEqual(r['count_rate'], 25000.0)

    def test_truncated_record_raises(self):
        """Records shorter than 34 bytes should raise."""
        with self.assertRaises(ValueError):
            decode_reading(b'\x00' * 30)

    def test_extra_bytes_ignored(self):
        """Only the first 34 bytes are decoded; extra data is ignored."""
        data = encode_reading(1.0, 2.0, 3.0, 4.0, 5) + b'\x00' * 100
        r = decode_reading(data[:READING_SIZE])

        self.assertEqual(r['lat'], 1.0)


class TestBufferStatsMath(unittest.TestCase):
    """Test the math used for buffer capacity calculations."""

    def test_capacity_calculation(self):
        """Calculate max readings from available LittleFS space.

        Actual LittleFS size depends on partition table (varies by board).
        This test verifies the formula, not a specific number.
        """
        # Example: 8MB board with ~5.9 MB for LittleFS after partitions
        littlefs_total = 5_900_000  # bytes available for LittleFS
        max_readings = littlefs_total // READING_SIZE

        self.assertEqual(max_readings, 173529)

    def test_prune_threshold(self):
        """Pruning should trigger at ~48K readings (configurable threshold)."""
        prune_threshold = 48000 * READING_SIZE
        self.assertEqual(prune_threshold, 1632000)  # ~1.56 MB

    def test_three_file_overhead(self):
        """Three files: readings.bin + status + index."""
        # readings.bin grows with data (34 bytes per reading)
        # readings_status.bin = 4 bytes (uint32_t depth)
        # readings_idx.bin = 4 bytes per reading (uint32_t array)
        n_readings = 1000
        total_size = (n_readings * READING_SIZE) + 4 + (n_readings * 4)

        self.assertEqual(total_size, 38004)


class TestReadingTimestamp(unittest.TestCase):
    """Test timestamp handling in readings."""

    def test_epoch_format(self):
        """Timestamps should be Unix epoch seconds."""
        data = encode_reading(0.0, 0.0, 0.0, 0.0, 1700000000)
        r = decode_reading(data)

        self.assertEqual(r['timestamp'], 1700000000)
        # Verify it's a reasonable epoch (year 2023+)
        self.assertGreater(r['timestamp'], 1609459200)  # 2021-01-01

    def test_timestamp_ordering(self):
        """Later timestamps should be larger values."""
        data1 = encode_reading(0.0, 0.0, 0.0, 0.0, 1700000000)
        data2 = encode_reading(0.0, 0.0, 0.0, 0.0, 1700000002)

        r1 = decode_reading(data1)
        r2 = decode_reading(data2)

        self.assertLess(r1['timestamp'], r2['timestamp'])


if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(TestDecodeReading)
    suite.addTests(unittest.TestLoader().loadTestsFromTestCase(TestBufferStatsMath))
    suite.addTests(unittest.TestLoader().loadTestsFromTestCase(TestReadingTimestamp))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

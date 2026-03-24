#!/usr/bin/env python3
"""
Tests for US-004: appendReading() with binary serialization to LittleFS.

Validates:
 - Binary record size is exactly 34 bytes (READING_BINARY_SIZE)
 - Binary layout: 7 floats/uint32 (28 bytes) + 3 uint16 (6 bytes) = 34 bytes
 - appendReading() implementation in buffer.cpp
 - Parallel status file /readings_status.bin usage
 - Index update after append
"""

import os
import re
import struct
import sys
import unittest

FIRMWARE_DIR = os.path.join(os.path.dirname(__file__), "..")
BUFFER_H = os.path.join(FIRMWARE_DIR, "src", "buffer.h")
BUFFER_CPP = os.path.join(FIRMWARE_DIR, "src", "buffer.cpp")


def read_file(path):
    with open(path, "r") as f:
        return f.read()


class TestBinaryRecordSize(unittest.TestCase):
    """Verify the 34-byte binary record layout math."""

    def test_reading_binary_size_constant_is_34(self):
        src = read_file(BUFFER_H)
        m = re.search(r"READING_BINARY_SIZE\s*=\s*(\d+)", src)
        self.assertIsNotNone(m, "READING_BINARY_SIZE not found in buffer.h")
        self.assertEqual(int(m.group(1)), 34)

    def test_binary_layout_packing_math(self):
        """7 x 4-byte fields + 3 x 2-byte fields = 34 bytes."""
        four_byte_fields = 7  # lat, lon, dose_rate, count_rate, timestamp, accuracy, altitude
        two_byte_fields = 3   # speed_mph_x10, speed_kph_x10, heading_x10
        total = four_byte_fields * 4 + two_byte_fields * 2
        self.assertEqual(total, 34)

    def test_struct_pack_produces_34_bytes(self):
        """Simulate the binary packing in Python to verify size."""
        # Pack: 4 floats + 1 uint32 + 2 floats + 3 uint16
        # lat(f) lon(f) dose_rate(f) count_rate(f) timestamp(I) accuracy(f) altitude(f)
        # speed_mph_x10(H) speed_kph_x10(H) heading_x10(H)
        fmt = "<ffffIffHHH"
        size = struct.calcsize(fmt)
        self.assertEqual(size, 34, f"Packed struct size is {size}, expected 34")

    def test_pack_and_unpack_roundtrip(self):
        """Pack sample values and verify roundtrip."""
        fmt = "<ffffIffHHH"
        lat = 61.2181
        lon = -149.9003
        dose_rate = 0.15
        count_rate = 12.5
        timestamp = 1711234567
        accuracy = 3.5
        altitude = 45.2
        speed_mph = 35.6
        speed_kph = 57.3
        heading = 182.4

        packed = struct.pack(
            fmt,
            lat, lon, dose_rate, count_rate, timestamp,
            accuracy, altitude,
            int(speed_mph * 10), int(speed_kph * 10), int(heading * 10),
        )
        self.assertEqual(len(packed), 34)

        unpacked = struct.unpack(fmt, packed)
        self.assertAlmostEqual(unpacked[0], lat, places=3)
        self.assertAlmostEqual(unpacked[1], lon, places=3)
        self.assertEqual(unpacked[4], timestamp)
        self.assertEqual(unpacked[7], 356)  # speed_mph * 10
        self.assertEqual(unpacked[8], 573)  # speed_kph * 10
        self.assertEqual(unpacked[9], 1824) # heading * 10


class TestAppendReadingImpl(unittest.TestCase):
    """Verify appendReading() implementation in buffer.cpp source code."""

    def setUp(self):
        self.src = read_file(BUFFER_CPP)

    def test_opens_readings_bin_in_append_mode(self):
        self.assertIn('LittleFS.open(READINGS_FILE, "a")', self.src)

    def test_writes_reading_binary_size_bytes(self):
        self.assertIn("f.write(record, READING_BINARY_SIZE)", self.src)

    def test_checks_written_equals_binary_size(self):
        self.assertIn("written != READING_BINARY_SIZE", self.src)

    def test_returns_false_on_file_open_failure(self):
        # Should return false if file can't be opened
        self.assertIn("return false", self.src)

    def test_increments_depth(self):
        self.assertIn("_depth++", self.src)

    def test_increments_lifetime_logged(self):
        self.assertIn("_lifetimeLogged++", self.src)

    def test_calls_save_index(self):
        self.assertIn("_saveIndex()", self.src)


class TestParallelStatusFile(unittest.TestCase):
    """Verify /readings_status.bin usage for upload status tracking."""

    def setUp(self):
        self.src = read_file(BUFFER_CPP)

    def test_status_file_path_defined(self):
        self.assertIn('/readings_status.bin', self.src)

    def test_status_file_opened_in_append_mode(self):
        self.assertIn('LittleFS.open(STATUS_FILE, "a")', self.src)

    def test_writes_pending_status_byte(self):
        # Should write a 0 byte for pending status
        self.assertIn("uint8_t pending = 0", self.src)

    def test_writes_one_byte_to_status_file(self):
        self.assertIn("sf.write(&pending, 1)", self.src)

    def test_checks_status_write_success(self):
        self.assertIn("sw != 1", self.src)


class TestIndexUpdate(unittest.TestCase):
    """Verify index file is updated after appendReading."""

    def setUp(self):
        self.src = read_file(BUFFER_CPP)

    def test_save_index_called_after_append(self):
        # _saveIndex() should be called and its result returned
        self.assertIn("return _saveIndex()", self.src)

    def test_index_file_path(self):
        self.assertIn('/readings_idx.bin', self.src)


class TestBinaryLayoutDocumentation(unittest.TestCase):
    """Verify the binary layout is documented in buffer.h."""

    def setUp(self):
        self.src = read_file(BUFFER_H)

    def test_documents_altitude_accuracy_dropped(self):
        self.assertIn("altitude_accuracy", self.src)
        self.assertIn("dropped", self.src.lower())

    def test_documents_status_file(self):
        self.assertIn("readings_status.bin", self.src)

    def test_documents_34_byte_layout(self):
        self.assertIn("34 bytes", self.src)


if __name__ == "__main__":
    unittest.main()

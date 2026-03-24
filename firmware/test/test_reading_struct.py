#!/usr/bin/env python3
"""Tests for Reading struct definition in buffer.h."""

import re
import sys
import os
import unittest

HEADER_PATH = os.path.join(
    os.path.dirname(__file__), '..', 'src', 'buffer.h'
)

class TestReadingStruct(unittest.TestCase):
    """Verify buffer.h defines the Reading struct and binary size constant."""

    @classmethod
    def setUpClass(cls):
        with open(HEADER_PATH, 'r') as f:
            cls.header = f.read()

    def test_file_exists(self):
        """buffer.h must exist."""
        self.assertTrue(os.path.isfile(HEADER_PATH), "buffer.h not found")

    def test_pragma_once(self):
        """Header should have #pragma once."""
        self.assertIn('#pragma once', self.header)

    def test_include_guard(self):
        """Header should have an include guard."""
        self.assertRegex(self.header, r'#ifndef\s+BUFFER_H')
        self.assertRegex(self.header, r'#define\s+BUFFER_H')
        self.assertRegex(self.header, r'#endif')

    def test_reading_binary_size_defined(self):
        """READING_BINARY_SIZE must be defined and equal 34."""
        match = re.search(r'READING_BINARY_SIZE\s*=\s*(\d+)', self.header)
        self.assertIsNotNone(match, "READING_BINARY_SIZE not found")
        self.assertEqual(int(match.group(1)), 34,
                         "READING_BINARY_SIZE must be 34")

    def test_struct_reading_exists(self):
        """struct Reading must be defined."""
        self.assertRegex(self.header, r'struct\s+Reading\s*\{')

    def test_all_fields_present(self):
        """All required fields must appear in the struct."""
        required_fields = [
            'id', 'lat', 'lon', 'dose_rate', 'count_rate',
            'timestamp', 'accuracy', 'altitude',
            'speed_mph', 'speed_kph', 'heading',
            'altitude_accuracy', 'uploaded',
        ]
        # Extract struct body
        match = re.search(
            r'struct\s+Reading\s*\{(.*?)\}', self.header, re.DOTALL
        )
        self.assertIsNotNone(match, "Could not parse struct Reading body")
        body = match.group(1)
        for field in required_fields:
            self.assertRegex(
                body, rf'\b{field}\b',
                f"Field '{field}' missing from struct Reading"
            )

    def test_field_types(self):
        """Key fields should have correct types."""
        body_match = re.search(
            r'struct\s+Reading\s*\{(.*?)\}', self.header, re.DOTALL
        )
        body = body_match.group(1)
        # uint32_t fields
        for field in ['id', 'timestamp']:
            self.assertRegex(body, rf'uint32_t\s+{field}\b',
                             f"{field} should be uint32_t")
        # float fields
        for field in ['lat', 'lon', 'dose_rate', 'count_rate',
                       'accuracy', 'altitude', 'speed_mph', 'speed_kph',
                       'heading', 'altitude_accuracy']:
            self.assertRegex(body, rf'float\s+{field}\b',
                             f"{field} should be float")
        # bool
        self.assertRegex(body, r'bool\s+uploaded\b',
                         "uploaded should be bool")

    def test_static_assert_present(self):
        """A static_assert on READING_BINARY_SIZE should exist."""
        self.assertRegex(
            self.header,
            r'static_assert\s*\(\s*READING_BINARY_SIZE\s*==\s*34',
            "static_assert for READING_BINARY_SIZE == 34 not found"
        )


if __name__ == '__main__':
    unittest.main()

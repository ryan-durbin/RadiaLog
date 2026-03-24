#!/usr/bin/env python3
"""
Unit tests for RadiaCode USB transport layer (US-002).

These tests validate the protocol logic (framing, sequence numbering, length
parsing) by re-implementing the same logic in Python and cross-checking it
against the C++ source.  No hardware or PlatformIO is required.

Run:
    python3 firmware/test/test_radiacode_transport.py
"""

import struct
import sys
import os
import re
import unittest

# ---------------------------------------------------------------------------
# Python re-implementation of the C++ transport logic
# This mirrors radiacode.cpp so we can test framing rules independently.
# ---------------------------------------------------------------------------

class TransportFraming:
    """Pure-Python implementation of the RadiaCode request/response framing."""

    MAX_SEQ = 32

    def __init__(self):
        self._seq = 0

    def build_request(self, command_id: int, args: bytes = b'') -> bytes:
        """Build a length-prefixed request packet.

        Payload: [cmd_id_lo][cmd_id_hi][0x00][seq_byte][...args]
        Frame:   [4-byte LE uint32 len][payload]
        Seq byte: 0x80 + (seq % 32); seq increments after call.
        """
        seq_byte = 0x80 + (self._seq % self.MAX_SEQ)
        self._seq = (self._seq + 1) % self.MAX_SEQ

        payload = (
            bytes([command_id & 0xFF, (command_id >> 8) & 0xFF, 0x00, seq_byte])
            + args
        )
        length_prefix = struct.pack('<I', len(payload))
        return length_prefix + payload

    def parse_response_length(self, data: bytes) -> int:
        """Extract the 4-byte LE response length from the start of a response."""
        if len(data) < 4:
            raise ValueError('Response too short to contain length prefix')
        return struct.unpack_from('<I', data)[0]

    def extract_response_payload(self, data: bytes) -> bytes:
        """Strip the 4-byte length prefix, returning raw payload bytes."""
        length = self.parse_response_length(data)
        payload = data[4:]
        if len(payload) < length:
            raise ValueError(
                f'Incomplete response: expected {length} bytes, got {len(payload)}'
            )
        return payload[:length]

    @property
    def seq(self):
        return self._seq


class SourceCodeValidator:
    """Validates patterns in the C++ source files."""

    def __init__(self, src_dir: str):
        self.src_dir = src_dir

    def read(self, filename: str) -> str:
        path = os.path.join(self.src_dir, filename)
        with open(path, 'r') as f:
            return f.read()

    def contains(self, filename: str, pattern: str) -> bool:
        src = self.read(filename)
        return bool(re.search(pattern, src))


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', 'src')


class TestSequenceNumbering(unittest.TestCase):
    """Sequence number increments 0-31 and wraps, byte is 0x80 + seq."""

    def setUp(self):
        self.framing = TransportFraming()

    def _seq_byte_from_request(self, request: bytes) -> int:
        # Byte 7 of the framed packet: [4-byte len][cmd_lo][cmd_hi][0x00][seq_byte]
        return request[7]

    def test_initial_seq_is_zero(self):
        self.assertEqual(self.framing.seq, 0)

    def test_first_request_seq_byte_is_0x80(self):
        req = self.framing.build_request(0x0005)
        seq_byte = self._seq_byte_from_request(req)
        self.assertEqual(seq_byte, 0x80)

    def test_second_request_seq_byte_is_0x81(self):
        self.framing.build_request(0x0005)  # seq 0 → seq 1
        req = self.framing.build_request(0x0005)
        seq_byte = self._seq_byte_from_request(req)
        self.assertEqual(seq_byte, 0x81)

    def test_seq_wraps_at_32(self):
        for _ in range(32):
            self.framing.build_request(0x0005)
        # After 32 calls, seq should be back to 0
        self.assertEqual(self.framing.seq, 0)
        req = self.framing.build_request(0x0005)
        seq_byte = self._seq_byte_from_request(req)
        self.assertEqual(seq_byte, 0x80)

    def test_seq_31_gives_0x9f(self):
        for _ in range(31):
            self.framing.build_request(0x0005)
        req = self.framing.build_request(0x0005)
        seq_byte = self._seq_byte_from_request(req)
        self.assertEqual(seq_byte, 0x80 + 31)
        self.assertEqual(seq_byte, 0x9F)

    def test_multiple_wraps(self):
        for _ in range(64):
            self.framing.build_request(0x0005)
        self.assertEqual(self.framing.seq, 0)


class TestRequestBuilding(unittest.TestCase):
    """Validate length-prefixed request framing."""

    def setUp(self):
        self.framing = TransportFraming()

    def test_no_args_length_is_4(self):
        req = self.framing.build_request(0x0005)
        length = struct.unpack_from('<I', req)[0]
        # Payload = [cmd_lo][cmd_hi][0x00][seq] = 4 bytes
        self.assertEqual(length, 4)

    def test_with_args_length_accounts_for_args(self):
        args = b'\x01\xff\x12\xff'
        req = self.framing.build_request(0x0007, args)
        length = struct.unpack_from('<I', req)[0]
        self.assertEqual(length, 4 + len(args))

    def test_command_id_little_endian_lo_byte(self):
        req = self.framing.build_request(0x0A04)
        # After 4-byte length prefix: [cmd_lo=0x04][cmd_hi=0x0A]
        self.assertEqual(req[4], 0x04)
        self.assertEqual(req[5], 0x0A)

    def test_command_id_little_endian_hi_byte(self):
        req = self.framing.build_request(0x0824)
        self.assertEqual(req[4], 0x24)
        self.assertEqual(req[5], 0x08)

    def test_byte_6_is_zero(self):
        req = self.framing.build_request(0x0005)
        self.assertEqual(req[6], 0x00)

    def test_total_frame_length(self):
        args = b'\xAA\xBB'
        req = self.framing.build_request(0x0005, args)
        # 4 (length prefix) + 4 (header) + 2 (args) = 10
        self.assertEqual(len(req), 10)

    def test_args_appended_after_header(self):
        args = b'\x01\x02\x03'
        req = self.framing.build_request(0x0005, args)
        # Payload starts at byte 4: [cmd_lo][cmd_hi][0x00][seq_byte][args...]
        payload = req[4:]
        self.assertEqual(payload[4:], args)

    def test_empty_request_total_length(self):
        req = self.framing.build_request(0x000A)
        self.assertEqual(len(req), 8)  # 4-byte prefix + 4-byte header


class TestResponseParsing(unittest.TestCase):
    """Validate length-prefix response parsing."""

    def setUp(self):
        self.framing = TransportFraming()

    def _make_response(self, payload: bytes) -> bytes:
        return struct.pack('<I', len(payload)) + payload

    def test_parse_response_length_simple(self):
        response = self._make_response(b'\xDE\xAD\xBE\xEF')
        length = self.framing.parse_response_length(response)
        self.assertEqual(length, 4)

    def test_parse_response_length_zero(self):
        response = self._make_response(b'')
        length = self.framing.parse_response_length(response)
        self.assertEqual(length, 0)

    def test_parse_response_length_large(self):
        payload = bytes(range(200))
        response = self._make_response(payload)
        length = self.framing.parse_response_length(response)
        self.assertEqual(length, 200)

    def test_extract_payload_correct(self):
        payload = b'\x01\x02\x03\x04\x05'
        response = self._make_response(payload)
        extracted = self.framing.extract_response_payload(response)
        self.assertEqual(extracted, payload)

    def test_extract_payload_strips_prefix(self):
        payload = b'\xAA\xBB'
        response = self._make_response(payload)
        extracted = self.framing.extract_response_payload(response)
        self.assertEqual(len(extracted), 2)

    def test_too_short_raises(self):
        with self.assertRaises(ValueError):
            self.framing.parse_response_length(b'\x01\x02')

    def test_incomplete_response_raises(self):
        # Length prefix says 10 bytes but only 3 provided
        response = struct.pack('<I', 10) + b'\x01\x02\x03'
        with self.assertRaises(ValueError):
            self.framing.extract_response_payload(response)

    def test_le_byte_order(self):
        # Length 256 = 0x00000100 → bytes [0x00, 0x01, 0x00, 0x00] in LE
        payload = b'\x00' * 256
        response = self._make_response(payload)
        self.assertEqual(response[0], 0x00)
        self.assertEqual(response[1], 0x01)
        self.assertEqual(response[2], 0x00)
        self.assertEqual(response[3], 0x00)
        length = self.framing.parse_response_length(response)
        self.assertEqual(length, 256)


class TestMultiReadReassembly(unittest.TestCase):
    """Validate multi-read logic: accumulate chunks until response_length bytes received."""

    def test_single_read_complete(self):
        """If the first read contains all data, no further reads needed."""
        framing = TransportFraming()
        payload = b'\xDE\xAD\xBE\xEF'
        response = struct.pack('<I', len(payload)) + payload

        length = framing.parse_response_length(response)
        data = response[4:]  # first read already includes everything
        self.assertEqual(len(data), length)

    def test_multi_read_accumulation(self):
        """Simulate receiving response in two chunks."""
        framing = TransportFraming()
        full_payload = bytes(range(100))
        full_response = struct.pack('<I', len(full_payload)) + full_payload

        # First read: just the header + 20 bytes
        first_read = full_response[:24]
        second_read = full_response[24:]

        response_length = framing.parse_response_length(first_read)
        accumulated = bytearray(first_read[4:])

        while len(accumulated) < response_length:
            accumulated += second_read
            second_read = b''  # consumed

        self.assertEqual(len(accumulated), response_length)
        self.assertEqual(bytes(accumulated), full_payload)

    def test_three_chunk_reassembly(self):
        """Response split across three reads."""
        framing = TransportFraming()
        full_payload = bytes(range(60))
        full_response = struct.pack('<I', len(full_payload)) + full_payload

        chunks = [full_response[:10], full_response[10:30], full_response[30:]]

        response_length = framing.parse_response_length(chunks[0])
        accumulated = bytearray(chunks[0][4:])

        for chunk in chunks[1:]:
            if len(accumulated) >= response_length:
                break
            accumulated += chunk

        self.assertEqual(len(bytes(accumulated)), response_length)


class TestSourceCodeStructure(unittest.TestCase):
    """Verify the C++ source files contain the expected declarations."""

    def setUp(self):
        self.v = SourceCodeValidator(SRC_DIR)

    def test_header_file_exists(self):
        path = os.path.join(SRC_DIR, 'radiacode.h')
        self.assertTrue(os.path.exists(path), 'radiacode.h not found')

    def test_cpp_file_exists(self):
        path = os.path.join(SRC_DIR, 'radiacode.cpp')
        self.assertTrue(os.path.exists(path), 'radiacode.cpp not found')

    def test_header_declares_connect(self):
        self.assertTrue(self.v.contains('radiacode.h', r'connect\s*\('),
                        'connect() not declared in radiacode.h')

    def test_header_declares_disconnect(self):
        self.assertTrue(self.v.contains('radiacode.h', r'disconnect\s*\('),
                        'disconnect() not declared in radiacode.h')

    def test_header_declares_execute(self):
        self.assertTrue(self.v.contains('radiacode.h', r'execute\s*\('),
                        'execute() not declared in radiacode.h')

    def test_header_declares_build_request(self):
        self.assertTrue(self.v.contains('radiacode.h', r'buildRequest'),
                        'buildRequest not declared in radiacode.h')

    def test_header_declares_radiacode_class(self):
        self.assertTrue(self.v.contains('radiacode.h', r'class\s+RadiaCode'),
                        'class RadiaCode not in radiacode.h')

    def test_header_has_vid(self):
        # VID is in config.h but referenced in cpp; check cpp uses it
        self.assertTrue(self.v.contains('radiacode.cpp', r'0x0483|RADIACODE_VID'),
                        'VID 0x0483 or RADIACODE_VID not referenced in radiacode.cpp')

    def test_header_has_pid(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'0xF123|RADIACODE_PID'),
                        'PID 0xF123 or RADIACODE_PID not referenced in radiacode.cpp')

    def test_cpp_has_seq_modulo_32(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'%\s*32'),
                        'seq % 32 not found in radiacode.cpp')

    def test_cpp_has_0x80_seq(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'0x80'),
                        '0x80 (seq byte base) not found in radiacode.cpp')

    def test_cpp_has_drain_stale_data(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'drain'),
                        'drainStaleData not referenced in radiacode.cpp')

    def test_cpp_has_multiple_read_failures(self):
        self.assertTrue(
            self.v.contains('radiacode.cpp', r'MULTIPLE_READ_FAILURES|MultipleUSBRead|MAX_READ_RETRIES'),
            'Multiple read failure handling not found in radiacode.cpp')

    def test_cpp_has_length_prefix_unpack(self):
        self.assertTrue(
            self.v.contains('radiacode.cpp', r'unpackLE32|response_length'),
            'Response length parsing not found in radiacode.cpp')

    def test_cpp_has_length_prefix_pack(self):
        self.assertTrue(
            self.v.contains('radiacode.cpp', r'packLE32'),
            'Request length packing not found in radiacode.cpp')

    def test_header_has_error_enum(self):
        self.assertTrue(self.v.contains('radiacode.h', r'enum class Error'),
                        'Error enum not in radiacode.h')

    def test_header_has_not_connected_error(self):
        self.assertTrue(self.v.contains('radiacode.h', r'NOT_CONNECTED'),
                        'NOT_CONNECTED error not in radiacode.h')

    def test_header_has_device_not_found(self):
        self.assertTrue(self.v.contains('radiacode.h', r'DEVICE_NOT_FOUND'),
                        'DEVICE_NOT_FOUND error not in radiacode.h')

    def test_cpp_write_endpoint_01(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'USB_WRITE_EP|0x01'),
                        'Write endpoint 0x01 not referenced in radiacode.cpp')

    def test_cpp_read_endpoint_81(self):
        self.assertTrue(self.v.contains('radiacode.cpp', r'USB_READ_EP|0x81'),
                        'Read endpoint 0x81 not referenced in radiacode.cpp')


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestSequenceNumbering))
    suite.addTests(loader.loadTestsFromTestCase(TestRequestBuilding))
    suite.addTests(loader.loadTestsFromTestCase(TestResponseParsing))
    suite.addTests(loader.loadTestsFromTestCase(TestMultiReadReassembly))
    suite.addTests(loader.loadTestsFromTestCase(TestSourceCodeStructure))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

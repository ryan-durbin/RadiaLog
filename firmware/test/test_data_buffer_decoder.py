#!/usr/bin/env python3
"""
US-005: Test RadiaCode data buffer decoder

Tests verify:
 1. RadiaCodeReading struct defined with required fields
 2. readDataBuf() method declared and implemented
 3. parseDataBuf() static helper declared and implemented
 4. 7-byte record headers are parsed correctly (seq, eid, gid, ts_offset)
 5. RealTimeData (gid=0) decoded from struct <ffHHHB format
 6. Timestamps computed as base_time + ts_offset * 10 ms
 7. Non-RealTimeData records (gid != 0) are skipped without error
 8. Empty buffer returns empty result list
 9. Python mirror of parseDataBuf produces correct readings from known payloads
"""

import struct
import sys
import os
import re
import unittest

# ─── Python mirror of the C++ data buffer parser ─────────────────────────────

HEADER_SIZE = 7          # seq(1) + eid(1) + gid(1) + ts_offset(4)
REALTIME_DATA_SIZE = 15  # count_rate(4f) + dose_rate(4f) + cre(2H) + dre(2H) + flags(2H) + rt_flags(1B)


class RadiaCodeReading:
    """Mirror of the C++ RadiaCodeReading struct."""
    def __init__(self, count_rate, dose_rate, timestamp, count_rate_err, dose_rate_err):
        self.count_rate = count_rate
        self.dose_rate = dose_rate
        self.timestamp = timestamp            # Unix seconds
        self.count_rate_err = count_rate_err
        self.dose_rate_err = dose_rate_err


def parse_data_buf(data: bytes, base_time_ms: int) -> list:
    """
    Mirror of RadiaCode::parseDataBuf().
    Parses raw data buffer bytes into a list of RadiaCodeReading.
    """
    readings = []
    if not data:
        return readings

    offset = 0
    while offset + HEADER_SIZE <= len(data):
        # 7-byte header
        rec_seq = data[offset]
        eid = data[offset + 1]
        gid = data[offset + 2]
        ts_offset = struct.unpack_from('<i', data, offset + 3)[0]  # signed int32 LE
        offset += HEADER_SIZE

        if gid == 0:
            # RealTimeData: struct <ffHHHB
            if offset + REALTIME_DATA_SIZE > len(data):
                break
            count_rate, dose_rate, count_rate_err, dose_rate_err, flags, rt_flags = \
                struct.unpack_from('<ffHHHB', data, offset)

            ts_ms = base_time_ms + ts_offset * 10
            timestamp = ts_ms // 1000

            readings.append(RadiaCodeReading(
                count_rate=count_rate,
                dose_rate=dose_rate,
                timestamp=timestamp,
                count_rate_err=count_rate_err,
                dose_rate_err=dose_rate_err,
            ))
            offset += REALTIME_DATA_SIZE
        else:
            # Unknown gid — can't determine payload size, stop parsing
            break

    return readings


def build_realtime_record(seq, eid, gid, ts_offset, count_rate, dose_rate,
                          count_rate_err=0, dose_rate_err=0, flags=0, rt_flags=0):
    """Build a single data buffer record with a RealTimeData payload."""
    header = struct.pack('<BBBi', seq, eid, gid, ts_offset)
    payload = struct.pack('<ffHHHB', count_rate, dose_rate,
                          count_rate_err, dose_rate_err, flags, rt_flags)
    return header + payload


def build_non_realtime_header(seq, eid, gid, ts_offset):
    """Build just a 7-byte header for a non-RealTimeData record."""
    return struct.pack('<BBBi', seq, eid, gid, ts_offset)


# ─── Source code path helper ─────────────────────────────────────────────────

def _src_path(rel_path: str) -> str:
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, 'src', rel_path)


def _read_src(rel_path: str) -> str:
    with open(_src_path(rel_path)) as f:
        return f.read()


# ─── Test Classes ─────────────────────────────────────────────────────────────

class TestRadiaCodeReadingStruct(unittest.TestCase):
    """Verify RadiaCodeReading struct is defined in radiacode.h."""

    def test_struct_defined(self):
        src = _read_src('radiacode.h')
        self.assertIn('struct RadiaCodeReading', src)

    def test_count_rate_field(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'float\s+count_rate')

    def test_dose_rate_field(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'float\s+dose_rate')

    def test_timestamp_field(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'uint32_t\s+timestamp')

    def test_count_rate_err_field(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'uint16_t\s+count_rate_err')

    def test_dose_rate_err_field(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'uint16_t\s+dose_rate_err')


class TestReadDataBufDeclaration(unittest.TestCase):
    """Verify readDataBuf() and parseDataBuf() are declared/implemented."""

    def test_readDataBuf_declared_in_header(self):
        src = _read_src('radiacode.h')
        self.assertIn('readDataBuf', src)

    def test_readDataBuf_returns_vector(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'std::vector\s*<\s*RadiaCodeReading\s*>\s+readDataBuf')

    def test_parseDataBuf_declared_in_header(self):
        src = _read_src('radiacode.h')
        self.assertIn('parseDataBuf', src)

    def test_parseDataBuf_is_static(self):
        src = _read_src('radiacode.h')
        self.assertRegex(src, r'static\s+std::vector\s*<\s*RadiaCodeReading\s*>\s+parseDataBuf')

    def test_readDataBuf_implemented_in_cpp(self):
        src = _read_src('radiacode.cpp')
        self.assertIn('readDataBuf', src)

    def test_parseDataBuf_implemented_in_cpp(self):
        src = _read_src('radiacode.cpp')
        self.assertIn('parseDataBuf', src)

    def test_reads_vs_data_buf(self):
        src = _read_src('radiacode.cpp')
        # readDataBuf should use VS::DATA_BUF
        self.assertIn('VS::DATA_BUF', src)

    def test_calls_read_request(self):
        src = _read_src('radiacode.cpp')
        # readDataBuf should call read_request
        self.assertIn('read_request(VS::DATA_BUF)', src)


class TestEmptyBuffer(unittest.TestCase):
    """Verify empty buffer returns empty result list."""

    def test_none_data(self):
        readings = parse_data_buf(b'', 1700000000000)
        self.assertEqual(len(readings), 0)

    def test_empty_bytes(self):
        readings = parse_data_buf(bytes(), 1700000000000)
        self.assertEqual(len(readings), 0)

    def test_too_short_for_header(self):
        # Less than 7 bytes — can't parse a header
        readings = parse_data_buf(b'\x00\x01\x02\x03\x04\x05', 1700000000000)
        self.assertEqual(len(readings), 0)


class TestSingleRealTimeRecord(unittest.TestCase):
    """Verify parsing a single RealTimeData record."""

    def setUp(self):
        self.base_time_ms = 1700000000 * 1000  # base_time in ms
        self.ts_offset = 100  # 100 * 10ms = 1000ms = 1 second
        self.record = build_realtime_record(
            seq=0, eid=0, gid=0,
            ts_offset=self.ts_offset,
            count_rate=12.5,
            dose_rate=0.15,
            count_rate_err=10,
            dose_rate_err=5,
            flags=0,
            rt_flags=0,
        )

    def test_parses_one_record(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        self.assertEqual(len(readings), 1)

    def test_count_rate(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        self.assertAlmostEqual(readings[0].count_rate, 12.5, places=2)

    def test_dose_rate(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        self.assertAlmostEqual(readings[0].dose_rate, 0.15, places=4)

    def test_timestamp(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        # base_time_ms + 100 * 10 = base_time_ms + 1000ms
        expected_ts = (self.base_time_ms + self.ts_offset * 10) // 1000
        self.assertEqual(readings[0].timestamp, expected_ts)

    def test_count_rate_err(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        self.assertEqual(readings[0].count_rate_err, 10)

    def test_dose_rate_err(self):
        readings = parse_data_buf(self.record, self.base_time_ms)
        self.assertEqual(readings[0].dose_rate_err, 5)


class TestMultipleRecords(unittest.TestCase):
    """Verify parsing multiple consecutive RealTimeData records."""

    def setUp(self):
        self.base_time_ms = 1700000000 * 1000

    def test_two_records(self):
        r1 = build_realtime_record(0, 0, 0, 100, 10.0, 0.1, 5, 3)
        r2 = build_realtime_record(1, 0, 0, 200, 20.0, 0.2, 8, 6)
        readings = parse_data_buf(r1 + r2, self.base_time_ms)
        self.assertEqual(len(readings), 2)

    def test_first_record_values(self):
        r1 = build_realtime_record(0, 0, 0, 100, 10.0, 0.1)
        r2 = build_realtime_record(1, 0, 0, 200, 20.0, 0.2)
        readings = parse_data_buf(r1 + r2, self.base_time_ms)
        self.assertAlmostEqual(readings[0].count_rate, 10.0, places=2)
        self.assertAlmostEqual(readings[0].dose_rate, 0.1, places=4)

    def test_second_record_values(self):
        r1 = build_realtime_record(0, 0, 0, 100, 10.0, 0.1)
        r2 = build_realtime_record(1, 0, 0, 200, 20.0, 0.2)
        readings = parse_data_buf(r1 + r2, self.base_time_ms)
        self.assertAlmostEqual(readings[1].count_rate, 20.0, places=2)
        self.assertAlmostEqual(readings[1].dose_rate, 0.2, places=4)

    def test_timestamps_differ(self):
        r1 = build_realtime_record(0, 0, 0, 100, 10.0, 0.1)
        r2 = build_realtime_record(1, 0, 0, 200, 20.0, 0.2)
        readings = parse_data_buf(r1 + r2, self.base_time_ms)
        # r1: base + 100*10ms = base + 1000ms → +1s
        # r2: base + 200*10ms = base + 2000ms → +2s
        self.assertEqual(readings[1].timestamp - readings[0].timestamp, 1)

    def test_five_records(self):
        data = b''
        for i in range(5):
            data += build_realtime_record(i, 0, 0, i * 100, float(i), float(i) * 0.01)
        readings = parse_data_buf(data, self.base_time_ms)
        self.assertEqual(len(readings), 5)


class TestTimestampComputation(unittest.TestCase):
    """Verify timestamps are computed as base_time + ts_offset * 10 ms."""

    def test_zero_offset(self):
        base_ms = 1700000000 * 1000
        record = build_realtime_record(0, 0, 0, 0, 1.0, 0.01)
        readings = parse_data_buf(record, base_ms)
        self.assertEqual(readings[0].timestamp, 1700000000)

    def test_positive_offset(self):
        base_ms = 1700000000 * 1000
        record = build_realtime_record(0, 0, 0, 500, 1.0, 0.01)
        readings = parse_data_buf(record, base_ms)
        # 500 * 10ms = 5000ms = 5 seconds
        self.assertEqual(readings[0].timestamp, 1700000005)

    def test_negative_offset(self):
        base_ms = 1700000000 * 1000
        record = build_realtime_record(0, 0, 0, -100, 1.0, 0.01)
        readings = parse_data_buf(record, base_ms)
        # -100 * 10ms = -1000ms = -1 second
        self.assertEqual(readings[0].timestamp, 1700000000 - 1)

    def test_large_offset(self):
        base_ms = 1700000000 * 1000
        record = build_realtime_record(0, 0, 0, 360000, 1.0, 0.01)
        readings = parse_data_buf(record, base_ms)
        # 360000 * 10ms = 3600000ms = 3600 seconds = 1 hour
        self.assertEqual(readings[0].timestamp, 1700000000 + 3600)


class TestNonRealTimeRecordSkipping(unittest.TestCase):
    """Verify non-RealTimeData records (gid != 0) are handled gracefully."""

    def setUp(self):
        self.base_time_ms = 1700000000 * 1000

    def test_gid_1_stops_parsing(self):
        # gid=1 record followed by gid=0 record
        # Since we can't know gid=1 payload size, parsing stops
        non_rt = build_non_realtime_header(0, 0, 1, 100)
        rt = build_realtime_record(1, 0, 0, 200, 10.0, 0.1)
        readings = parse_data_buf(non_rt + rt, self.base_time_ms)
        self.assertEqual(len(readings), 0)

    def test_gid_0_before_gid_1(self):
        # gid=0 record followed by gid=1 — should get the first record
        rt = build_realtime_record(0, 0, 0, 100, 10.0, 0.1)
        non_rt = build_non_realtime_header(1, 0, 1, 200)
        readings = parse_data_buf(rt + non_rt, self.base_time_ms)
        self.assertEqual(len(readings), 1)
        self.assertAlmostEqual(readings[0].count_rate, 10.0, places=2)

    def test_gid_2_stops_parsing(self):
        non_rt = build_non_realtime_header(0, 0, 2, 100)
        readings = parse_data_buf(non_rt, self.base_time_ms)
        self.assertEqual(len(readings), 0)

    def test_gid_255_stops_parsing(self):
        non_rt = build_non_realtime_header(0, 0, 255, 100)
        readings = parse_data_buf(non_rt, self.base_time_ms)
        self.assertEqual(len(readings), 0)


class TestHeaderParsing(unittest.TestCase):
    """Verify 7-byte record headers are parsed correctly."""

    def setUp(self):
        self.base_time_ms = 1700000000 * 1000

    def test_seq_byte_preserved(self):
        # seq field in header doesn't affect output directly,
        # but parsing should proceed correctly with various seq values
        for seq_val in [0, 1, 127, 255]:
            record = build_realtime_record(seq_val, 0, 0, 0, 1.0, 0.01)
            readings = parse_data_buf(record, self.base_time_ms)
            self.assertEqual(len(readings), 1, f"Failed for seq={seq_val}")

    def test_eid_zero_accepted(self):
        record = build_realtime_record(0, 0, 0, 0, 1.0, 0.01)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertEqual(len(readings), 1)

    def test_eid_nonzero_with_gid_zero(self):
        # eid is event ID, gid=0 still means RealTimeData
        record = build_realtime_record(0, 5, 0, 0, 1.0, 0.01)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertEqual(len(readings), 1)

    def test_truncated_header_ignored(self):
        # Only 5 bytes — not enough for 7-byte header
        readings = parse_data_buf(b'\x00\x00\x00\x00\x00', self.base_time_ms)
        self.assertEqual(len(readings), 0)

    def test_truncated_payload_after_header(self):
        # Full header but not enough bytes for RealTimeData payload (need 15)
        header = struct.pack('<BBBi', 0, 0, 0, 100)
        partial_payload = b'\x00' * 10  # Only 10 of 15 needed bytes
        readings = parse_data_buf(header + partial_payload, self.base_time_ms)
        self.assertEqual(len(readings), 0)


class TestFloatDecoding(unittest.TestCase):
    """Verify float values are decoded correctly from binary data."""

    def setUp(self):
        self.base_time_ms = 1700000000 * 1000

    def test_zero_values(self):
        record = build_realtime_record(0, 0, 0, 0, 0.0, 0.0)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertAlmostEqual(readings[0].count_rate, 0.0)
        self.assertAlmostEqual(readings[0].dose_rate, 0.0)

    def test_large_count_rate(self):
        record = build_realtime_record(0, 0, 0, 0, 99999.5, 0.01)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertAlmostEqual(readings[0].count_rate, 99999.5, places=0)

    def test_small_dose_rate(self):
        record = build_realtime_record(0, 0, 0, 0, 1.0, 0.001)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertAlmostEqual(readings[0].dose_rate, 0.001, places=4)

    def test_max_uint16_errors(self):
        record = build_realtime_record(0, 0, 0, 0, 1.0, 0.01, 0xFFFF, 0xFFFF)
        readings = parse_data_buf(record, self.base_time_ms)
        self.assertEqual(readings[0].count_rate_err, 0xFFFF)
        self.assertEqual(readings[0].dose_rate_err, 0xFFFF)


class TestKnownBinaryPayload(unittest.TestCase):
    """Test with a hand-crafted known binary payload to verify exact decoding."""

    def test_known_payload(self):
        # Construct a known payload:
        # Record 1: seq=0, eid=0, gid=0, ts_offset=1000
        #   count_rate=42.5, dose_rate=0.123, cre=15, dre=7, flags=0x0001, rt_flags=0x02
        base_time_ms = 1700000000 * 1000

        header = struct.pack('<BBBi', 0, 0, 0, 1000)
        payload = struct.pack('<ffHHHB', 42.5, 0.123, 15, 7, 0x0001, 0x02)
        data = header + payload

        readings = parse_data_buf(data, base_time_ms)

        self.assertEqual(len(readings), 1)
        r = readings[0]
        self.assertAlmostEqual(r.count_rate, 42.5, places=2)
        self.assertAlmostEqual(r.dose_rate, 0.123, places=3)
        self.assertEqual(r.count_rate_err, 15)
        self.assertEqual(r.dose_rate_err, 7)
        # timestamp: base_time + 1000 * 10ms = base_time + 10000ms = base_time + 10s
        self.assertEqual(r.timestamp, 1700000000 + 10)

    def test_known_multi_record_payload(self):
        base_time_ms = 1500000000 * 1000

        r1 = build_realtime_record(0, 0, 0, 0, 5.5, 0.05, 2, 1)
        r2 = build_realtime_record(1, 0, 0, 6000, 15.3, 0.12, 4, 3)
        data = r1 + r2

        readings = parse_data_buf(data, base_time_ms)

        self.assertEqual(len(readings), 2)

        self.assertAlmostEqual(readings[0].count_rate, 5.5, places=1)
        self.assertAlmostEqual(readings[0].dose_rate, 0.05, places=3)
        self.assertEqual(readings[0].timestamp, 1500000000)

        self.assertAlmostEqual(readings[1].count_rate, 15.3, places=1)
        self.assertAlmostEqual(readings[1].dose_rate, 0.12, places=3)
        # 6000 * 10ms = 60000ms = 60s
        self.assertEqual(readings[1].timestamp, 1500000000 + 60)


class TestCppSourceStructure(unittest.TestCase):
    """Validate C++ source code structure for data buffer decoder."""

    def test_header_size_7(self):
        src = _read_src('radiacode.cpp')
        self.assertRegex(src, r'HEADER_SIZE\s*=\s*7')

    def test_realtime_data_size_15(self):
        src = _read_src('radiacode.cpp')
        self.assertRegex(src, r'REALTIME_DATA_SIZE\s*=\s*15')

    def test_gid_check_for_zero(self):
        src = _read_src('radiacode.cpp')
        self.assertIn('gid == 0', src)

    def test_ts_offset_10ms_multiplication(self):
        src = _read_src('radiacode.cpp')
        # Should multiply ts_offset by 10 for milliseconds
        self.assertRegex(src, r'ts_offset.*10')

    def test_memcpy_for_float(self):
        src = _read_src('radiacode.cpp')
        # Should use memcpy for safe float reading from buffer
        self.assertIn('memcpy', src)
        self.assertIn('count_rate', src)
        self.assertIn('dose_rate', src)

    def test_returns_vector_of_readings(self):
        src = _read_src('radiacode.cpp')
        self.assertIn('std::vector<RadiaCodeReading>', src)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestRadiaCodeReadingStruct))
    suite.addTests(loader.loadTestsFromTestCase(TestReadDataBufDeclaration))
    suite.addTests(loader.loadTestsFromTestCase(TestEmptyBuffer))
    suite.addTests(loader.loadTestsFromTestCase(TestSingleRealTimeRecord))
    suite.addTests(loader.loadTestsFromTestCase(TestMultipleRecords))
    suite.addTests(loader.loadTestsFromTestCase(TestTimestampComputation))
    suite.addTests(loader.loadTestsFromTestCase(TestNonRealTimeRecordSkipping))
    suite.addTests(loader.loadTestsFromTestCase(TestHeaderParsing))
    suite.addTests(loader.loadTestsFromTestCase(TestFloatDecoding))
    suite.addTests(loader.loadTestsFromTestCase(TestKnownBinaryPayload))
    suite.addTests(loader.loadTestsFromTestCase(TestCppSourceStructure))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

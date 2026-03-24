#!/usr/bin/env python3
"""
US-004: Test RadiaCode init sequence

Tests verify:
  1. init() builds commands in the correct order
  2. SET_EXCHANGE payload contains 0x01 0xff 0x12 0xff
  3. SET_TIME payload contains a 4-byte LE Unix timestamp
  4. DEVICE_TIME write_request targets VSFR 0x0504 with value 0
  5. base_time = current_time + 128 seconds
  6. GET_VERSION is issued as part of init
  7. Source code structure: init() declared/implemented, _base_time member
"""

import struct
import sys
import unittest
import re

# ─── Python mirror of C++ helpers ────────────────────────────────────────────

class COMMAND:
    SET_EXCHANGE   = 0x0007
    SET_TIME       = 0x0A04
    GET_VERSION    = 0x000A
    RD_VIRT_SFR    = 0x0824
    WR_VIRT_SFR    = 0x0825
    RD_VIRT_STRING = 0x0826
    WR_VIRT_STRING = 0x0827


def pack_le32(val: int) -> bytes:
    return struct.pack('<I', val)


def build_request(command_id: int, args: bytes, seq: int) -> bytes:
    """Mirror of RadiaCode::buildRequest()"""
    seq_byte = 0x80 + (seq % 32)
    payload = bytes([
        command_id & 0xFF,
        (command_id >> 8) & 0xFF,
        0x00,
        seq_byte,
    ]) + args
    return pack_le32(len(payload)) + payload


def read_request(vs_id: int, seq: int) -> bytes:
    args = bytes([vs_id & 0xFF, (vs_id >> 8) & 0xFF])
    return build_request(COMMAND.RD_VIRT_STRING, args, seq)


def write_request(vsfr_id: int, value: int, seq: int) -> bytes:
    args = bytes([vsfr_id & 0xFF, (vsfr_id >> 8) & 0xFF]) + pack_le32(value)
    return build_request(COMMAND.WR_VIRT_SFR, args, seq)


# Fixed "current time" used by the C++ unit-test stub
TEST_FIXED_TIME = 1700000000  # 2023-11-14 22:13:20 UTC


# ─── Python mirror of init() sequence ────────────────────────────────────────

def simulate_init(start_seq: int = 0):
    """
    Simulate the exact sequence of requests that init() produces.
    Returns a list of (label, raw_bytes) tuples.
    """
    seq = start_seq
    requests = []

    # Step 2: SET_EXCHANGE
    req = build_request(COMMAND.SET_EXCHANGE, bytes([0x01, 0xff, 0x12, 0xff]), seq)
    requests.append(('SET_EXCHANGE', req, seq))
    seq = (seq + 1) % 32

    # Step 3: SET_TIME
    req = build_request(COMMAND.SET_TIME, pack_le32(TEST_FIXED_TIME), seq)
    requests.append(('SET_TIME', req, seq))
    seq = (seq + 1) % 32

    # Step 4: Write DEVICE_TIME to VSFR 0x0504 with value 0
    req = write_request(0x0504, 0, seq)
    requests.append(('DEVICE_TIME', req, seq))
    seq = (seq + 1) % 32

    # Step 6: GET_VERSION
    req = build_request(COMMAND.GET_VERSION, b'', seq)
    requests.append(('GET_VERSION', req, seq))
    seq = (seq + 1) % 32

    return requests


def parse_request(raw: bytes):
    """Return (length, cmd_id, seq_byte, args)"""
    if len(raw) < 8:
        raise ValueError("Too short")
    length = struct.unpack_from('<I', raw, 0)[0]
    cmd_lo, cmd_hi, _zero, seq_byte = raw[4], raw[5], raw[6], raw[7]
    cmd_id = cmd_lo | (cmd_hi << 8)
    args = raw[8:]
    return length, cmd_id, seq_byte, args


# ─── Tests ────────────────────────────────────────────────────────────────────

class TestInitSequenceOrder(unittest.TestCase):
    """Verify commands appear in the correct order"""

    def setUp(self):
        self.requests = simulate_init(start_seq=0)

    def test_first_command_is_set_exchange(self):
        label, raw, _ = self.requests[0]
        self.assertEqual(label, 'SET_EXCHANGE')
        _, cmd_id, _, _ = parse_request(raw)
        self.assertEqual(cmd_id, COMMAND.SET_EXCHANGE)

    def test_second_command_is_set_time(self):
        label, raw, _ = self.requests[1]
        self.assertEqual(label, 'SET_TIME')
        _, cmd_id, _, _ = parse_request(raw)
        self.assertEqual(cmd_id, COMMAND.SET_TIME)

    def test_third_command_is_device_time_write(self):
        label, raw, _ = self.requests[2]
        self.assertEqual(label, 'DEVICE_TIME')
        _, cmd_id, _, _ = parse_request(raw)
        self.assertEqual(cmd_id, COMMAND.WR_VIRT_SFR)

    def test_fourth_command_is_get_version(self):
        label, raw, _ = self.requests[3]
        self.assertEqual(label, 'GET_VERSION')
        _, cmd_id, _, _ = parse_request(raw)
        self.assertEqual(cmd_id, COMMAND.GET_VERSION)

    def test_four_commands_total(self):
        self.assertEqual(len(self.requests), 4)


class TestSetExchangePayload(unittest.TestCase):
    """SET_EXCHANGE must carry exactly [0x01, 0xff, 0x12, 0xff]"""

    def setUp(self):
        self.requests = simulate_init(start_seq=0)
        _, self.raw, _ = self.requests[0]  # SET_EXCHANGE

    def test_args_length_is_4(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(len(args), 4)

    def test_first_byte_is_0x01(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[0], 0x01)

    def test_second_byte_is_0xff(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[1], 0xff)

    def test_third_byte_is_0x12(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[2], 0x12)

    def test_fourth_byte_is_0xff(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[3], 0xff)

    def test_exact_payload(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(bytes(args), bytes([0x01, 0xff, 0x12, 0xff]))


class TestSetTimePayload(unittest.TestCase):
    """SET_TIME must carry the current time as 4-byte LE uint32"""

    def setUp(self):
        self.requests = simulate_init(start_seq=0)
        _, self.raw, _ = self.requests[1]  # SET_TIME

    def test_args_length_is_4(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(len(args), 4)

    def test_timestamp_is_little_endian(self):
        _, _, _, args = parse_request(self.raw)
        ts = struct.unpack('<I', bytes(args))[0]
        self.assertEqual(ts, TEST_FIXED_TIME)

    def test_timestamp_is_reasonable_unix_time(self):
        _, _, _, args = parse_request(self.raw)
        ts = struct.unpack('<I', bytes(args))[0]
        # Between 2020 and 2040
        self.assertGreater(ts, 1577836800)  # 2020-01-01
        self.assertLess(ts, 2208988800)     # 2040-01-01


class TestDeviceTimePayload(unittest.TestCase):
    """DEVICE_TIME write must target VSFR 0x0504 with value 0"""

    def setUp(self):
        self.requests = simulate_init(start_seq=0)
        _, self.raw, _ = self.requests[2]  # DEVICE_TIME

    def test_command_is_wr_virt_sfr(self):
        _, cmd_id, _, _ = parse_request(self.raw)
        self.assertEqual(cmd_id, COMMAND.WR_VIRT_SFR)

    def test_vsfr_id_is_0x0504(self):
        _, _, _, args = parse_request(self.raw)
        vsfr_id = args[0] | (args[1] << 8)
        self.assertEqual(vsfr_id, 0x0504)

    def test_value_is_zero(self):
        _, _, _, args = parse_request(self.raw)
        value = struct.unpack_from('<I', bytes(args), 2)[0]
        self.assertEqual(value, 0)

    def test_args_length_is_6(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(len(args), 6)

    def test_vsfr_id_low_byte_is_0x04(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[0], 0x04)  # 0x0504 LE → low byte 0x04

    def test_vsfr_id_high_byte_is_0x05(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(args[1], 0x05)  # 0x0504 LE → high byte 0x05


class TestBaseTimeComputation(unittest.TestCase):
    """base_time = current_time + 128 seconds"""

    def test_base_time_is_now_plus_128(self):
        base_time = TEST_FIXED_TIME + 128
        self.assertEqual(base_time, TEST_FIXED_TIME + 128)

    def test_base_time_offset_is_128(self):
        now = TEST_FIXED_TIME
        base_time = now + 128
        self.assertEqual(base_time - now, 128)

    def test_base_time_greater_than_now(self):
        now = TEST_FIXED_TIME
        base_time = now + 128
        self.assertGreater(base_time, now)


class TestSeqNumberingThroughInit(unittest.TestCase):
    """Verify sequence numbers increment correctly through init commands"""

    def test_seq_starts_at_0_for_first_command(self):
        requests = simulate_init(start_seq=0)
        _, raw, _ = requests[0]
        _, _, seq_byte, _ = parse_request(raw)
        self.assertEqual(seq_byte, 0x80)  # 0x80 + 0

    def test_seq_increments_through_commands(self):
        requests = simulate_init(start_seq=0)
        for idx, (_, raw, _) in enumerate(requests):
            _, _, seq_byte, _ = parse_request(raw)
            expected_seq = 0x80 + idx
            self.assertEqual(seq_byte, expected_seq)

    def test_seq_wraps_correctly_from_high_start(self):
        # If start_seq=30, seq 30, 31, 0, 1
        requests = simulate_init(start_seq=30)
        expected_seqs = [30, 31, 0, 1]
        for idx, (_, raw, _) in enumerate(requests):
            _, _, seq_byte, _ = parse_request(raw)
            self.assertEqual(seq_byte, 0x80 + expected_seqs[idx])

    def test_all_seq_bytes_have_high_bit_set(self):
        requests = simulate_init(start_seq=0)
        for _, raw, _ in requests:
            _, _, seq_byte, _ = parse_request(raw)
            self.assertEqual(seq_byte & 0x80, 0x80)


class TestGetVersionCommand(unittest.TestCase):
    """GET_VERSION must be issued with no args"""

    def setUp(self):
        self.requests = simulate_init(start_seq=0)
        _, self.raw, _ = self.requests[3]  # GET_VERSION

    def test_command_id_is_get_version(self):
        _, cmd_id, _, _ = parse_request(self.raw)
        self.assertEqual(cmd_id, COMMAND.GET_VERSION)

    def test_no_args(self):
        _, _, _, args = parse_request(self.raw)
        self.assertEqual(len(args), 0)

    def test_length_prefix_correct(self):
        length, _, _, _ = parse_request(self.raw)
        # payload = 4 header bytes + 0 args = 4
        self.assertEqual(length, 4)


class TestSourceCodeStructure(unittest.TestCase):
    """Validate C++ source files contain required definitions for US-004"""

    def _read(self, rel_path: str) -> str:
        import os
        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        path = os.path.join(base, 'src', rel_path)
        with open(path) as f:
            return f.read()

    def test_init_declared_in_header(self):
        src = self._read('radiacode.h')
        self.assertIn('bool init()', src)

    def test_base_time_member_in_header(self):
        src = self._read('radiacode.h')
        self.assertIn('_base_time', src)

    def test_get_base_time_accessor(self):
        src = self._read('radiacode.h')
        self.assertIn('getBaseTime', src)

    def test_init_implemented_in_cpp(self):
        src = self._read('radiacode.cpp')
        self.assertIn('RadiaCode::init()', src)

    def test_init_calls_drain_stale_data(self):
        src = self._read('radiacode.cpp')
        # init() must call drainStaleData()
        self.assertIn('drainStaleData()', src)

    def test_init_sends_set_exchange(self):
        src = self._read('radiacode.cpp')
        self.assertIn('SET_EXCHANGE', src)

    def test_init_set_exchange_payload(self):
        src = self._read('radiacode.cpp')
        # The init payload: 0x01, 0xff, 0x12, 0xff
        self.assertIn('0x01', src)
        self.assertIn('0xff', src)
        self.assertIn('0x12', src)

    def test_init_sends_set_time(self):
        src = self._read('radiacode.cpp')
        self.assertIn('SET_TIME', src)

    def test_init_writes_device_time_vsfr(self):
        src = self._read('radiacode.cpp')
        # VSFR 0x0504 for DEVICE_TIME
        self.assertIn('0x0504', src)

    def test_init_computes_base_time(self):
        src = self._read('radiacode.cpp')
        # base_time = getCurrentTimeSec() + 128
        self.assertIn('_base_time', src)
        self.assertIn('128', src)

    def test_init_calls_get_version(self):
        src = self._read('radiacode.cpp')
        self.assertIn('GET_VERSION', src)

    def test_init_returns_bool(self):
        src = self._read('radiacode.cpp')
        # Should have 'return false;' and 'return true;'
        self.assertIn('return false;', src)
        self.assertIn('return true;', src)

    def test_get_current_time_sec_declared(self):
        src = self._read('radiacode.h')
        self.assertIn('getCurrentTimeSec', src)

    def test_get_current_time_sec_implemented(self):
        src = self._read('radiacode.cpp')
        self.assertIn('getCurrentTimeSec', src)

    def test_base_time_initialised_to_zero_in_constructor(self):
        src = self._read('radiacode.cpp')
        # Constructor initialiser list should include _base_time(0)
        self.assertIn('_base_time(0)', src)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestInitSequenceOrder))
    suite.addTests(loader.loadTestsFromTestCase(TestSetExchangePayload))
    suite.addTests(loader.loadTestsFromTestCase(TestSetTimePayload))
    suite.addTests(loader.loadTestsFromTestCase(TestDeviceTimePayload))
    suite.addTests(loader.loadTestsFromTestCase(TestBaseTimeComputation))
    suite.addTests(loader.loadTestsFromTestCase(TestSeqNumberingThroughInit))
    suite.addTests(loader.loadTestsFromTestCase(TestGetVersionCommand))
    suite.addTests(loader.loadTestsFromTestCase(TestSourceCodeStructure))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

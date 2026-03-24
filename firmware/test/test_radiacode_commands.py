#!/usr/bin/env python3
"""
US-003: Test RadiaCode command helpers (read/write requests)

Tests verify:
 1. COMMAND enum values match protocol spec
 2. VS enum values match protocol spec
 3. read_request() builds correct RD_VIRT_STRING payload
 4. write_request() builds correct WR_VIRT_SFR payload
 5. sendCommand() signature / delegation
 6. Source code structure
"""

import struct
import sys
import unittest
import re

# ─── Python mirror of the C++ transport helpers ──────────────────────────────

class COMMAND:
    SET_EXCHANGE   = 0x0007
    SET_TIME       = 0x0A04
    GET_VERSION    = 0x000A
    GET_SERIAL     = 0x000B
    RD_VIRT_SFR    = 0x0824
    WR_VIRT_SFR    = 0x0825
    RD_VIRT_STRING = 0x0826
    WR_VIRT_STRING = 0x0827


class VS:
    CONFIGURATION = 0x0002
    SERIAL_NUMBER = 0x0008
    DATA_BUF      = 0x0100
    SPECTRUM      = 0x0200


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
    """Mirror of RadiaCode::read_request()"""
    args = bytes([vs_id & 0xFF, (vs_id >> 8) & 0xFF])
    return build_request(COMMAND.RD_VIRT_STRING, args, seq)


def write_request(vsfr_id: int, value: int, seq: int) -> bytes:
    """Mirror of RadiaCode::write_request()"""
    args = bytes([vsfr_id & 0xFF, (vsfr_id >> 8) & 0xFF]) + pack_le32(value)
    return build_request(COMMAND.WR_VIRT_SFR, args, seq)


# ─── Test Classes ─────────────────────────────────────────────────────────────

class TestCommandEnum(unittest.TestCase):
    """Verify COMMAND enum values match protocol spec"""

    def test_set_exchange(self):
        self.assertEqual(COMMAND.SET_EXCHANGE, 0x0007)

    def test_set_time(self):
        self.assertEqual(COMMAND.SET_TIME, 0x0A04)

    def test_get_version(self):
        self.assertEqual(COMMAND.GET_VERSION, 0x000A)

    def test_get_serial(self):
        self.assertEqual(COMMAND.GET_SERIAL, 0x000B)

    def test_rd_virt_sfr(self):
        self.assertEqual(COMMAND.RD_VIRT_SFR, 0x0824)

    def test_wr_virt_sfr(self):
        self.assertEqual(COMMAND.WR_VIRT_SFR, 0x0825)

    def test_rd_virt_string(self):
        self.assertEqual(COMMAND.RD_VIRT_STRING, 0x0826)

    def test_wr_virt_string(self):
        self.assertEqual(COMMAND.WR_VIRT_STRING, 0x0827)


class TestVSEnum(unittest.TestCase):
    """Verify VS enum values match protocol spec"""

    def test_configuration(self):
        self.assertEqual(VS.CONFIGURATION, 0x0002)

    def test_serial_number(self):
        self.assertEqual(VS.SERIAL_NUMBER, 0x0008)

    def test_data_buf(self):
        self.assertEqual(VS.DATA_BUF, 0x0100)

    def test_spectrum(self):
        self.assertEqual(VS.SPECTRUM, 0x0200)


class TestReadRequest(unittest.TestCase):
    """Verify read_request() builds correct RD_VIRT_STRING payload"""

    def _parse(self, raw: bytes):
        """Return (length, cmd_id, zero_byte, seq_byte, args)"""
        self.assertGreaterEqual(len(raw), 8)
        length = struct.unpack_from('<I', raw, 0)[0]
        cmd_lo, cmd_hi, zero, seq = raw[4], raw[5], raw[6], raw[7]
        cmd_id = cmd_lo | (cmd_hi << 8)
        args = raw[8:]
        return length, cmd_id, zero, seq, args

    def test_command_id_is_rd_virt_string(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        _, cmd_id, _, _, _ = self._parse(raw)
        self.assertEqual(cmd_id, COMMAND.RD_VIRT_STRING)

    def test_length_prefix_matches_payload(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        length, _, _, _, _ = self._parse(raw)
        self.assertEqual(length, len(raw) - 4)

    def test_seq_byte_first(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        _, _, _, seq_byte, _ = self._parse(raw)
        self.assertEqual(seq_byte, 0x80)  # 0x80 + 0

    def test_seq_byte_increments(self):
        for i in range(5):
            raw = read_request(VS.DATA_BUF, seq=i)
            _, _, _, seq_byte, _ = self._parse(raw)
            self.assertEqual(seq_byte, 0x80 + i)

    def test_seq_byte_wraps_at_32(self):
        raw = read_request(VS.DATA_BUF, seq=32)
        _, _, _, seq_byte, _ = self._parse(raw)
        self.assertEqual(seq_byte, 0x80)  # wraps back to 0x80

    def test_zero_byte_present(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        _, _, zero, _, _ = self._parse(raw)
        self.assertEqual(zero, 0x00)

    def test_args_contain_vs_id_le(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        _, _, _, _, args = self._parse(raw)
        self.assertEqual(len(args), 2)
        vs_id_from_args = args[0] | (args[1] << 8)
        self.assertEqual(vs_id_from_args, VS.DATA_BUF)

    def test_data_buf_vs_id_bytes(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        # DATA_BUF = 0x0100 → LE bytes [0x00, 0x01]
        self.assertEqual(raw[8], 0x00)
        self.assertEqual(raw[9], 0x01)

    def test_serial_number_vs_id_bytes(self):
        raw = read_request(VS.SERIAL_NUMBER, seq=0)
        # SERIAL_NUMBER = 0x0008 → LE bytes [0x08, 0x00]
        self.assertEqual(raw[8], 0x08)
        self.assertEqual(raw[9], 0x00)

    def test_configuration_vs_id_bytes(self):
        raw = read_request(VS.CONFIGURATION, seq=0)
        # CONFIGURATION = 0x0002 → LE bytes [0x02, 0x00]
        self.assertEqual(raw[8], 0x02)
        self.assertEqual(raw[9], 0x00)

    def test_total_length(self):
        raw = read_request(VS.DATA_BUF, seq=0)
        # 4 (length prefix) + 4 (header) + 2 (vs_id args) = 10 bytes
        self.assertEqual(len(raw), 10)


class TestWriteRequest(unittest.TestCase):
    """Verify write_request() builds correct WR_VIRT_SFR payload"""

    def _parse(self, raw: bytes):
        length = struct.unpack_from('<I', raw, 0)[0]
        cmd_lo, cmd_hi, zero, seq = raw[4], raw[5], raw[6], raw[7]
        cmd_id = cmd_lo | (cmd_hi << 8)
        args = raw[8:]
        return length, cmd_id, zero, seq, args

    def test_command_id_is_wr_virt_sfr(self):
        raw = write_request(0x0504, 0xDEADBEEF, seq=0)
        _, cmd_id, _, _, _ = self._parse(raw)
        self.assertEqual(cmd_id, COMMAND.WR_VIRT_SFR)

    def test_length_prefix_matches_payload(self):
        raw = write_request(0x0504, 0x12345678, seq=0)
        length, _, _, _, _ = self._parse(raw)
        self.assertEqual(length, len(raw) - 4)

    def test_args_contain_vsfr_id_le(self):
        raw = write_request(0x0504, 0, seq=0)
        _, _, _, _, args = self._parse(raw)
        vsfr_id_from_args = args[0] | (args[1] << 8)
        self.assertEqual(vsfr_id_from_args, 0x0504)

    def test_args_contain_value_le32(self):
        val = 0xDEADBEEF
        raw = write_request(0x0504, val, seq=0)
        _, _, _, _, args = self._parse(raw)
        value_from_args = struct.unpack_from('<I', args, 2)[0]
        self.assertEqual(value_from_args, val)

    def test_args_contain_zero_value(self):
        raw = write_request(0x0504, 0, seq=0)
        _, _, _, _, args = self._parse(raw)
        self.assertEqual(len(args), 6)
        self.assertEqual(args[2:], b'\x00\x00\x00\x00')

    def test_seq_byte_correct(self):
        raw = write_request(0x0504, 0, seq=5)
        _, _, _, seq_byte, _ = self._parse(raw)
        self.assertEqual(seq_byte, 0x80 + 5)

    def test_total_length(self):
        raw = write_request(0x0504, 0x1234, seq=0)
        # 4 (length prefix) + 4 (header) + 2 (vsfr_id) + 4 (value) = 14 bytes
        self.assertEqual(len(raw), 14)

    def test_device_time_vsfr_id_bytes(self):
        raw = write_request(0x0504, 0, seq=0)
        # 0x0504 → LE bytes [0x04, 0x05]
        self.assertEqual(raw[8], 0x04)
        self.assertEqual(raw[9], 0x05)


class TestSendCommandSeqNumbering(unittest.TestCase):
    """Verify sendCommand (build_request) seq numbering across calls"""

    def _seq_from_raw(self, raw: bytes) -> int:
        return raw[7] - 0x80

    def test_seq_starts_at_zero(self):
        raw = build_request(COMMAND.GET_VERSION, b'', seq=0)
        self.assertEqual(self._seq_from_raw(raw), 0)

    def test_seq_increments_each_call(self):
        for i in range(10):
            raw = build_request(COMMAND.GET_VERSION, b'', seq=i)
            self.assertEqual(self._seq_from_raw(raw), i)

    def test_seq_wraps_at_32(self):
        for i in range(40):
            raw = build_request(COMMAND.GET_VERSION, b'', seq=i)
            expected = i % 32
            self.assertEqual(self._seq_from_raw(raw), expected)

    def test_seq_byte_high_bit(self):
        # seq_byte should always have bit 7 set
        for i in range(32):
            raw = build_request(COMMAND.GET_VERSION, b'', seq=i)
            self.assertEqual(raw[7] & 0x80, 0x80)


class TestSourceCodeStructure(unittest.TestCase):
    """Validate C++ source files contain required definitions"""

    def _read(self, rel_path: str) -> str:
        import os
        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        path = os.path.join(base, 'src', rel_path)
        with open(path) as f:
            return f.read()

    def test_command_enum_defined(self):
        src = self._read('radiacode.h')
        self.assertIn('enum class COMMAND', src)

    def test_vs_enum_defined(self):
        src = self._read('radiacode.h')
        self.assertIn('enum class VS', src)

    def test_command_set_exchange(self):
        src = self._read('radiacode.h')
        self.assertIn('SET_EXCHANGE', src)
        self.assertIn('0x0007', src)

    def test_command_set_time(self):
        src = self._read('radiacode.h')
        self.assertIn('SET_TIME', src)
        self.assertIn('0x0A04', src)

    def test_command_get_version(self):
        src = self._read('radiacode.h')
        self.assertIn('GET_VERSION', src)
        self.assertIn('0x000A', src)

    def test_command_get_serial(self):
        src = self._read('radiacode.h')
        self.assertIn('GET_SERIAL', src)
        self.assertIn('0x000B', src)

    def test_command_rd_virt_sfr(self):
        src = self._read('radiacode.h')
        self.assertIn('RD_VIRT_SFR', src)
        self.assertIn('0x0824', src)

    def test_command_wr_virt_sfr(self):
        src = self._read('radiacode.h')
        self.assertIn('WR_VIRT_SFR', src)
        self.assertIn('0x0825', src)

    def test_command_rd_virt_string(self):
        src = self._read('radiacode.h')
        self.assertIn('RD_VIRT_STRING', src)
        self.assertIn('0x0826', src)

    def test_vs_data_buf(self):
        src = self._read('radiacode.h')
        self.assertIn('DATA_BUF', src)
        self.assertIn('0x0100', src)

    def test_vs_configuration(self):
        src = self._read('radiacode.h')
        self.assertIn('CONFIGURATION', src)
        self.assertIn('0x0002', src)

    def test_vs_serial_number(self):
        src = self._read('radiacode.h')
        self.assertIn('SERIAL_NUMBER', src)
        self.assertIn('0x0008', src)

    def test_vs_spectrum(self):
        src = self._read('radiacode.h')
        self.assertIn('SPECTRUM', src)
        self.assertIn('0x0200', src)

    def test_read_request_declared(self):
        src = self._read('radiacode.h')
        self.assertIn('read_request', src)

    def test_write_request_declared(self):
        src = self._read('radiacode.h')
        self.assertIn('write_request', src)

    def test_send_command_declared(self):
        src = self._read('radiacode.h')
        self.assertIn('sendCommand', src)

    def test_read_request_implemented(self):
        src = self._read('radiacode.cpp')
        self.assertIn('read_request', src)
        self.assertIn('RD_VIRT_STRING', src)

    def test_write_request_implemented(self):
        src = self._read('radiacode.cpp')
        self.assertIn('write_request', src)
        self.assertIn('WR_VIRT_SFR', src)

    def test_send_command_implemented(self):
        src = self._read('radiacode.cpp')
        self.assertIn('sendCommand', src)

    def test_send_command_calls_execute(self):
        src = self._read('radiacode.cpp')
        # sendCommand should call execute()
        self.assertIn('execute(', src)

    def test_send_command_calls_build_request(self):
        src = self._read('radiacode.cpp')
        self.assertIn('buildRequest(', src)


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestCommandEnum))
    suite.addTests(loader.loadTestsFromTestCase(TestVSEnum))
    suite.addTests(loader.loadTestsFromTestCase(TestReadRequest))
    suite.addTests(loader.loadTestsFromTestCase(TestWriteRequest))
    suite.addTests(loader.loadTestsFromTestCase(TestSendCommandSeqNumbering))
    suite.addTests(loader.loadTestsFromTestCase(TestSourceCodeStructure))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

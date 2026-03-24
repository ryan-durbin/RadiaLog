#!/usr/bin/env python3
"""
US-002: Wire setup() — config, debug logging, LittleFS, and buffer initialization
Tests that main.cpp includes the correct headers, declares global instances,
and calls init functions in the correct order within setup().
"""

import os
import re
import sys
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAIN_CPP = os.path.join(REPO_ROOT, "firmware", "src", "main.cpp")


def read_main():
    with open(MAIN_CPP, "r") as f:
        return f.read()


def get_setup_body(src):
    """Extract the body of setup() function."""
    match = re.search(r'void\s+setup\s*\(\s*\)\s*\{', src)
    if not match:
        return ""
    start = match.end()
    depth = 1
    i = start
    while i < len(src) and depth > 0:
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
        i += 1
    return src[start:i - 1]


def find_line_number(text, pattern):
    """Return the line number (1-indexed) of the first match, or -1."""
    for i, line in enumerate(text.split('\n'), 1):
        if re.search(pattern, line):
            return i
    return -1


class TestIncludes(unittest.TestCase):
    """AC1: main.cpp includes config_mgr.h, portal/debug_ws.h, buffer.h, and reading.h"""

    def setUp(self):
        self.src = read_main()

    def test_include_config_mgr(self):
        self.assertRegex(self.src, r'#include\s+"config_mgr\.h"')

    def test_include_debug_ws(self):
        self.assertRegex(self.src, r'#include\s+"portal/debug_ws\.h"')

    def test_include_buffer(self):
        self.assertRegex(self.src, r'#include\s+"buffer\.h"')

    def test_include_reading(self):
        self.assertRegex(self.src, r'#include\s+"reading\.h"')


class TestGlobalInstances(unittest.TestCase):
    """AC2: Global ConfigMgr and ReadingBuffer instances at file scope"""

    def setUp(self):
        self.src = read_main()
        # Get code outside of function bodies (rough: before first void setup)
        setup_pos = self.src.find('void setup()')
        self.file_scope = self.src[:setup_pos] if setup_pos > 0 else self.src

    def test_global_configmgr(self):
        """ConfigMgr instance declared at file scope (not static or inside function)"""
        self.assertRegex(self.file_scope, r'(?:^|\n)\s*ConfigMgr\s+\w+\s*;',
                         "Expected ConfigMgr instance at file scope")

    def test_global_configmgr_named_configMgr(self):
        """ConfigMgr instance should be named configMgr"""
        self.assertRegex(self.file_scope, r'ConfigMgr\s+configMgr\s*;')

    def test_global_readingbuffer(self):
        """ReadingBuffer instance declared at file scope"""
        self.assertRegex(self.file_scope, r'(?:^|\n)\s*ReadingBuffer\s+\w+\s*;',
                         "Expected ReadingBuffer instance at file scope")

    def test_global_readingbuffer_named(self):
        """ReadingBuffer instance should be named readingBuffer"""
        self.assertRegex(self.file_scope, r'ReadingBuffer\s+readingBuffer\s*;')


class TestSetupOrder(unittest.TestCase):
    """AC3-6: setup() calls configMgr.load(), DebugWS::begin(), readingBuffer.begin()
    in the correct order, and logs buffer stats."""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())
        self.assertGreater(len(self.setup_body), 0, "Could not find setup() body")

    def _line_of(self, pattern):
        n = find_line_number(self.setup_body, pattern)
        self.assertGreater(n, 0, f"Pattern not found in setup(): {pattern}")
        return n

    def test_serial_begin_present(self):
        """Serial.begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'Serial\.begin\(')

    def test_serial_begin_first(self):
        """Serial.begin() comes before configMgr.load()"""
        serial_line = self._line_of(r'Serial\.begin\(')
        config_line = self._line_of(r'configMgr\.load\(\)')
        self.assertLess(serial_line, config_line,
                        "Serial.begin() must come before configMgr.load()")

    def test_config_load_before_debugws(self):
        """AC3: configMgr.load() comes before debugWS.begin()"""
        config_line = self._line_of(r'configMgr\.load\(\)')
        debug_line = self._line_of(r'debugWS\.begin\(')
        self.assertLess(config_line, debug_line,
                        "configMgr.load() must come before debugWS.begin()")

    def test_debugws_begin_present(self):
        """AC4: debugWS.begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'debugWS\.begin\(')

    def test_debugws_after_serial(self):
        """AC4: debugWS.begin() comes after Serial.begin()"""
        serial_line = self._line_of(r'Serial\.begin\(')
        debug_line = self._line_of(r'debugWS\.begin\(')
        self.assertLess(serial_line, debug_line,
                        "debugWS.begin() must come after Serial.begin()")

    def test_readingbuffer_begin_after_debugws(self):
        """AC5: readingBuffer.begin() comes after debugWS.begin()"""
        debug_line = self._line_of(r'debugWS\.begin\(')
        buffer_line = self._line_of(r'readingBuffer\.begin\(\)')
        self.assertLess(debug_line, buffer_line,
                        "readingBuffer.begin() must come after debugWS.begin()")

    def test_config_load_before_buffer(self):
        """configMgr.load() comes before readingBuffer.begin()"""
        config_line = self._line_of(r'configMgr\.load\(\)')
        buffer_line = self._line_of(r'readingBuffer\.begin\(\)')
        self.assertLess(config_line, buffer_line,
                        "configMgr.load() must come before readingBuffer.begin()")

    def test_buffer_stats_logged(self):
        """AC6: Buffer stats are logged via debugWS after readingBuffer.begin()"""
        # Check that getStats() is called after readingBuffer.begin()
        buffer_line = self._line_of(r'readingBuffer\.begin\(\)')
        stats_line = self._line_of(r'readingBuffer\.getStats\(\)')
        self.assertLess(buffer_line, stats_line,
                        "readingBuffer.getStats() must come after readingBuffer.begin()")

    def test_buffer_stats_via_debugws(self):
        """AC6: Buffer stats are logged using debugWS.log()"""
        # There should be a debugWS.log() call near the getStats() call
        self.assertRegex(self.setup_body, r'debugWS\.log\(.*[Bb]uffer.*[Ss]tored',
                         "Expected debugWS.log() with buffer stats info")

    def test_config_load_before_wifi(self):
        """AC3: configMgr.load() comes before wifi.begin() (wifi reads config)"""
        config_line = self._line_of(r'configMgr\.load\(\)')
        wifi_line = self._line_of(r'wifi\.begin\(')
        self.assertLess(config_line, wifi_line,
                        "configMgr.load() must come before wifi.begin()")


class TestDebugWSUsage(unittest.TestCase):
    """Verify debugWS.log() is used for logging after init (not raw Serial.printf)"""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())

    def test_debugws_log_used_after_init(self):
        """debugWS.log() is used for log messages after debugWS.begin()"""
        debug_begin_line = find_line_number(self.setup_body, r'debugWS\.begin\(')
        self.assertGreater(debug_begin_line, 0)
        # After debugWS.begin(), there should be debugWS.log() calls
        lines_after = self.setup_body.split('\n')[debug_begin_line:]
        log_calls = [l for l in lines_after if 'debugWS.log(' in l]
        self.assertGreater(len(log_calls), 0,
                           "Expected debugWS.log() calls after debugWS.begin()")

    def test_no_serial_printf_after_debugws(self):
        """No raw Serial.printf() calls after debugWS.begin()"""
        debug_begin_line = find_line_number(self.setup_body, r'debugWS\.begin\(')
        self.assertGreater(debug_begin_line, 0)
        lines_after = self.setup_body.split('\n')[debug_begin_line:]
        serial_prints = [l for l in lines_after
                         if re.search(r'Serial\.printf?\(', l)
                         and not l.strip().startswith('//')]
        self.assertEqual(len(serial_prints), 0,
                         f"Found raw Serial.print after debugWS.begin(): {serial_prints}")


class TestLoopUsesCorrectNames(unittest.TestCase):
    """Verify loop() uses configMgr and readingBuffer (not old names)"""

    def setUp(self):
        self.src = read_main()

    def test_loop_uses_configMgr(self):
        """loop() references configMgr for reading interval"""
        # Find loop body
        match = re.search(r'void\s+loop\s*\(\s*\)\s*\{', self.src)
        self.assertIsNotNone(match)
        loop_start = match.end()
        self.assertIn('configMgr.getReadingIntervalMs()', self.src[loop_start:])

    def test_loop_uses_readingBuffer(self):
        """loop() references readingBuffer for appending"""
        match = re.search(r'void\s+loop\s*\(\s*\)\s*\{', self.src)
        self.assertIsNotNone(match)
        loop_start = match.end()
        self.assertIn('readingBuffer.appendReading(', self.src[loop_start:])


if __name__ == "__main__":
    unittest.main(verbosity=2)

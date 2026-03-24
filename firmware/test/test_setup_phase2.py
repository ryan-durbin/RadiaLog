#!/usr/bin/env python3
"""
US-003: Wire setup() — WiFi, portal, uploader, RadiaCode, GPS, and LED initialization
Tests that main.cpp includes the correct headers, declares global instances,
and calls init functions in the correct order within setup() for phase 2 modules.
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


class TestPhase2Includes(unittest.TestCase):
    """AC1: main.cpp includes wifi_mgr.h, portal/portal.h, uploader.h,
    radiacode.h, gps/atgm336h.h, and led.h"""

    def setUp(self):
        self.src = read_main()

    def test_include_wifi_mgr(self):
        self.assertRegex(self.src, r'#include\s+"wifi_mgr\.h"')

    def test_include_portal(self):
        self.assertRegex(self.src, r'#include\s+"portal/portal\.h"')

    def test_include_uploader(self):
        self.assertRegex(self.src, r'#include\s+"uploader\.h"')

    def test_include_radiacode(self):
        self.assertRegex(self.src, r'#include\s+"radiacode\.h"')

    def test_include_gps_atgm336h(self):
        self.assertRegex(self.src, r'#include\s+"gps/atgm336h\.h"')

    def test_include_led(self):
        self.assertRegex(self.src, r'#include\s+"led\.h"')


class TestPhase2GlobalInstances(unittest.TestCase):
    """AC2: Global instances declared for WifiMgr, StatusPortal, Uploader,
    RadiaCode, ATGM336H, and Led"""

    def setUp(self):
        self.src = read_main()
        setup_pos = self.src.find('void setup()')
        self.file_scope = self.src[:setup_pos] if setup_pos > 0 else self.src

    def test_global_wifimgr(self):
        """WifiMgr instance declared at file scope"""
        self.assertRegex(self.file_scope, r'WifiMgr\s+\w+\s*[;(]',
                         "Expected WifiMgr instance at file scope")

    def test_global_statusportal(self):
        """StatusPortal instance declared at file scope"""
        self.assertRegex(self.file_scope, r'StatusPortal\s+\w+\s*;',
                         "Expected StatusPortal instance at file scope")

    def test_global_uploader(self):
        """Uploader instance declared at file scope"""
        self.assertRegex(self.file_scope, r'Uploader\s+\w+\s*;',
                         "Expected Uploader instance at file scope")

    def test_global_radiacode(self):
        """RadiaCode instance declared at file scope (may be namespaced)"""
        self.assertRegex(self.file_scope, r'(?:radiacode::)?RadiaCode\s+\w+',
                         "Expected RadiaCode instance at file scope")

    def test_global_atgm336h(self):
        """ATGM336H instance declared at file scope"""
        self.assertRegex(self.file_scope, r'ATGM336H\s+\w+',
                         "Expected ATGM336H instance at file scope")

    def test_global_led(self):
        """Led instance declared at file scope"""
        self.assertRegex(self.file_scope, r'Led\s+\w+\s*;',
                         "Expected Led instance at file scope")


class TestPhase2SetupCalls(unittest.TestCase):
    """AC3-7: setup() calls begin() on all phase 2 modules"""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())
        self.assertGreater(len(self.setup_body), 0, "Could not find setup() body")

    def _line_of(self, pattern):
        n = find_line_number(self.setup_body, pattern)
        self.assertGreater(n, 0, f"Pattern not found in setup(): {pattern}")
        return n

    def test_wifi_begin_present(self):
        """AC3: WifiMgr begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'wifi\w*\.begin\(')

    def test_portal_begin_present(self):
        """AC4: StatusPortal begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'portal\.begin\(')

    def test_portal_begin_with_args(self):
        """AC4: portal.begin() receives configMgr, readingBuffer, and radiaCode"""
        self.assertRegex(self.setup_body,
                         r'portal\.begin\(\s*configMgr\s*,\s*readingBuffer\s*,\s*radiaCode\s*\)')

    def test_uploader_begin_present(self):
        """AC5: Uploader begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'uploader\.begin\(')

    def test_radiacode_begin_or_connect_present(self):
        """AC6: RadiaCode begin() or connect() is called in setup()"""
        self.assertRegex(self.setup_body, r'radiaCode\.\w+\(',
                         "Expected radiaCode method call in setup()")

    def test_gps_begin_present(self):
        """AC6: ATGM336H begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'gps\.begin\(\)')

    def test_led_begin_present(self):
        """AC7: Led begin() is called in setup()"""
        self.assertRegex(self.setup_body, r'led\.begin\(\)')

    def test_led_set_pattern_present(self):
        """AC7: Led setPattern() is called in setup() for initial state"""
        self.assertRegex(self.setup_body, r'led\.setPattern\(')


class TestPhase2InitOrder(unittest.TestCase):
    """AC8: Init order in setup() is WiFi → Portal → Uploader → RadiaCode → GPS → LED"""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())
        self.assertGreater(len(self.setup_body), 0, "Could not find setup() body")

    def _line_of(self, pattern):
        n = find_line_number(self.setup_body, pattern)
        self.assertGreater(n, 0, f"Pattern not found in setup(): {pattern}")
        return n

    def test_buffer_before_wifi(self):
        """readingBuffer.begin() comes before wifi begin()"""
        buf_line = self._line_of(r'readingBuffer\.begin\(\)')
        wifi_line = self._line_of(r'^\s*wifi\w*\.begin\(')
        self.assertLess(buf_line, wifi_line,
                        "readingBuffer.begin() must come before wifi.begin()")

    def test_wifi_before_portal(self):
        """WiFi init comes before portal init"""
        wifi_line = self._line_of(r'^\s*wifi\w*\.begin\(')
        portal_line = self._line_of(r'^\s*portal\.begin\(')
        self.assertLess(wifi_line, portal_line,
                        "wifi.begin() must come before portal.begin()")

    def test_portal_before_uploader(self):
        """Portal init comes before uploader init"""
        portal_line = self._line_of(r'portal\.begin\(')
        uploader_line = self._line_of(r'uploader\.begin\(')
        self.assertLess(portal_line, uploader_line,
                        "portal.begin() must come before uploader.begin()")

    def test_uploader_before_radiacode(self):
        """Uploader init comes before RadiaCode init"""
        uploader_line = self._line_of(r'uploader\.begin\(')
        radiacode_line = self._line_of(r'radiaCode\.\w+\(')
        self.assertLess(uploader_line, radiacode_line,
                        "uploader.begin() must come before radiaCode init")

    def test_radiacode_before_gps(self):
        """RadiaCode init comes before GPS init"""
        radiacode_line = self._line_of(r'radiaCode\.\w+\(')
        gps_line = self._line_of(r'gps\.begin\(\)')
        self.assertLess(radiacode_line, gps_line,
                        "radiaCode init must come before gps.begin()")

    def test_gps_before_led(self):
        """GPS init comes before LED init"""
        gps_line = self._line_of(r'gps\.begin\(\)')
        led_line = self._line_of(r'led\.begin\(\)')
        self.assertLess(gps_line, led_line,
                        "gps.begin() must come before led.begin()")


class TestPhase2CompletionLog(unittest.TestCase):
    """AC9: setup() ends with a completion log message via DebugWS"""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())
        self.assertGreater(len(self.setup_body), 0, "Could not find setup() body")

    def test_completion_log_present(self):
        """Setup ends with a debugWS.log() completion message"""
        self.assertRegex(self.setup_body, r'debugWS\.log\(.*[Ss]etup\s+complete',
                         "Expected completion log message via debugWS at end of setup()")

    def test_completion_log_after_led(self):
        """Completion log comes after LED init"""
        lines = self.setup_body.split('\n')
        led_line = find_line_number(self.setup_body, r'led\.begin\(\)')
        self.assertGreater(led_line, 0)
        # Find last debugWS.log line
        last_log_line = -1
        for i, line in enumerate(lines, 1):
            if re.search(r'debugWS\.log\(', line):
                last_log_line = i
        self.assertGreater(last_log_line, led_line,
                           "Final debugWS.log() must come after led.begin()")


class TestPhase2LedPatternOnInit(unittest.TestCase):
    """AC7: LED pattern set based on initial USB/GPS state"""

    def setUp(self):
        self.setup_body = get_setup_body(read_main())

    def test_led_pattern_uses_ledpattern_enum(self):
        """setPattern uses LedPattern enum values"""
        self.assertRegex(self.setup_body, r'led\.setPattern\(\s*LedPattern::',
                         "Expected led.setPattern(LedPattern::...) in setup()")

    def test_led_pattern_conditional(self):
        """LED pattern choice depends on radiaCode connection state"""
        self.assertRegex(self.setup_body, r'radiaCode\.isConnected\(\)',
                         "Expected radiaCode.isConnected() check for LED pattern")


if __name__ == "__main__":
    unittest.main(verbosity=2)

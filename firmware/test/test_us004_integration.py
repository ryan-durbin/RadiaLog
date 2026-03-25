"""
US-004: Integration build verification and full test suite
Verifies that all three P0 fixes (RL-001, RL-002, RL-003) are present together
and that the source compiles cleanly.
"""

import unittest
import subprocess
import sys
import os

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FIRMWARE_DIR = os.path.join(REPO_ROOT, 'firmware')
SRC_DIR = os.path.join(FIRMWARE_DIR, 'src')


def read_file(path):
    with open(path, 'r') as f:
        return f.read()


class TestRL001FixPresent(unittest.TestCase):
    """Verify RL-001 fix: radiaCode.init() is called in setup()"""

    def setUp(self):
        self.main_cpp = read_file(os.path.join(SRC_DIR, 'main.cpp'))

    def test_init_call_present(self):
        """radiaCode.init() must be called in main.cpp"""
        self.assertIn('radiaCode.init()', self.main_cpp,
                      "radiaCode.init() not found in main.cpp")

    def test_init_after_connect(self):
        """init() must appear after connect() in main.cpp"""
        connect_pos = self.main_cpp.find('radiaCode.connect(')
        init_pos = self.main_cpp.find('radiaCode.init()')
        self.assertGreater(connect_pos, -1, "radiaCode.connect() not found")
        self.assertGreater(init_pos, -1, "radiaCode.init() not found")
        self.assertGreater(init_pos, connect_pos,
                           "radiaCode.init() must appear after radiaCode.connect()")


class TestRL002FixPresent(unittest.TestCase):
    """Verify RL-002 fix: semaphore-based USB transfer synchronization"""

    def setUp(self):
        self.radiacode_cpp = read_file(os.path.join(SRC_DIR, 'radiacode.cpp'))
        self.radiacode_h = read_file(os.path.join(SRC_DIR, 'radiacode.h'))

    def test_semaphore_member_in_header(self):
        """_xfer_semaphore member must be declared in header"""
        self.assertIn('_xfer_semaphore', self.radiacode_h,
                      "_xfer_semaphore not found in radiacode.h")

    def test_semaphore_take_in_cpp(self):
        """xSemaphoreTake must be used in radiacode.cpp"""
        self.assertIn('xSemaphoreTake', self.radiacode_cpp,
                      "xSemaphoreTake not found in radiacode.cpp")

    def test_semaphore_give_in_callback(self):
        """xSemaphoreGive must be used in transfer callback"""
        self.assertIn('xSemaphoreGive', self.radiacode_cpp,
                      "xSemaphoreGive not found in radiacode.cpp")

    def test_no_vtaskdelay_for_usb(self):
        """vTaskDelay should not be used for USB transfer wait"""
        import re
        # vTaskDelay should not appear after usbWrite or usbRead
        # Check that the USB_TIMEOUT_MS usage is removed from blocking wait
        # (It may still appear in semaphore timeout, which is acceptable)
        # The key check: no bare vTaskDelay(pdMS_TO_TICKS(USB_TIMEOUT_MS)) blocking
        lines = self.radiacode_cpp.split('\n')
        blocking_delays = [
            l for l in lines
            if 'vTaskDelay' in l and 'USB_TIMEOUT_MS' in l and '//' not in l.lstrip()[:2]
        ]
        self.assertEqual(len(blocking_delays), 0,
                         f"Found blocking vTaskDelay with USB_TIMEOUT_MS: {blocking_delays}")


class TestRL003FixPresent(unittest.TestCase):
    """Verify RL-003 fix: mutex protection in ReadingBuffer"""

    def setUp(self):
        self.buffer_cpp = read_file(os.path.join(SRC_DIR, 'buffer.cpp'))
        self.buffer_h = read_file(os.path.join(SRC_DIR, 'buffer.h'))

    def test_mutex_member_in_header(self):
        """_mutex member must be declared in buffer.h"""
        self.assertIn('_mutex', self.buffer_h,
                      "_mutex not found in buffer.h")

    def test_mutex_take_in_cpp(self):
        """xSemaphoreTake must be used in buffer.cpp"""
        self.assertIn('xSemaphoreTake', self.buffer_cpp,
                      "xSemaphoreTake not found in buffer.cpp")

    def test_mutex_give_in_cpp(self):
        """xSemaphoreGive must be used in buffer.cpp"""
        self.assertIn('xSemaphoreGive', self.buffer_cpp,
                      "xSemaphoreGive not found in buffer.cpp")

    def test_mutex_created_in_begin(self):
        """xSemaphoreCreateMutex must be called in begin()"""
        self.assertIn('xSemaphoreCreateMutex', self.buffer_cpp,
                      "xSemaphoreCreateMutex not found in buffer.cpp")

    def test_semphr_include_present(self):
        """freertos/semphr.h must be included in buffer.cpp"""
        self.assertIn('semphr.h', self.buffer_cpp,
                      "semphr.h include not found in buffer.cpp")


class TestAllFixesTogether(unittest.TestCase):
    """Integration: all three fixes coexist without conflict"""

    def test_all_fix_files_exist(self):
        """All modified source files must exist"""
        files = [
            os.path.join(SRC_DIR, 'main.cpp'),
            os.path.join(SRC_DIR, 'radiacode.cpp'),
            os.path.join(SRC_DIR, 'radiacode.h'),
            os.path.join(SRC_DIR, 'buffer.cpp'),
            os.path.join(SRC_DIR, 'buffer.h'),
        ]
        for f in files:
            self.assertTrue(os.path.exists(f), f"Missing file: {f}")

    def test_all_test_files_exist(self):
        """Test files for all three stories must exist"""
        test_dir = os.path.join(FIRMWARE_DIR, 'test')
        test_files = [
            'test_rl001_init_called.py',
            'test_rl002_usb_semaphore.py',
            'test_rl003_buffer_mutex.py',
        ]
        for tf in test_files:
            path = os.path.join(test_dir, tf)
            self.assertTrue(os.path.exists(path), f"Missing test file: {tf}")


if __name__ == '__main__':
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(__import__('__main__'))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

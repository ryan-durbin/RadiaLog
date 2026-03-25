#!/usr/bin/env python3
"""
test_rl003_buffer_mutex.py

Tests for RL-003: FreeRTOS mutex added to ReadingBuffer for thread-safe file I/O.

Verifies:
- buffer.h declares _mutex private member
- buffer.cpp constructor initializes _mutex to nullptr
- buffer.cpp begin() creates mutex with xSemaphoreCreateMutex()
- appendReading(), getUnuploaded(), markUploaded(), pruneUploaded() take/give mutex
- getStats() and _saveIndex() do NOT independently take the mutex
"""

import sys
import os
import re
import unittest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BUFFER_H   = os.path.join(REPO_ROOT, 'firmware', 'src', 'buffer.h')
BUFFER_CPP = os.path.join(REPO_ROOT, 'firmware', 'src', 'buffer.cpp')


def read_file(path):
    with open(path, 'r') as f:
        return f.read()


def extract_function_body(source, func_name):
    """
    Extract the body of a C++ function (between the first { and its matching })
    given the function name. Returns the body string, or None if not found.
    """
    # Find the start of the function definition
    pattern = re.compile(
        r'\b' + re.escape(func_name) + r'\b[^{;]*\{',
        re.DOTALL
    )
    m = pattern.search(source)
    if not m:
        return None

    start = m.end() - 1  # position of the opening {
    depth = 0
    i = start
    while i < len(source):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[start:i+1]
        i += 1
    return None


class TestBufferHMutexMember(unittest.TestCase):
    def setUp(self):
        self.header = read_file(BUFFER_H)

    def test_mutex_member_declared(self):
        """buffer.h must declare void* _mutex private member"""
        self.assertIn('void*', self.header,
                      "buffer.h should contain 'void*' type")
        self.assertIn('_mutex', self.header,
                      "buffer.h should declare _mutex member")

    def test_mutex_member_in_private_section(self):
        """_mutex declaration should appear in the private: section"""
        private_idx = self.header.find('private:')
        self.assertNotEqual(private_idx, -1,
                            "buffer.h should have a private: section")
        mutex_idx = self.header.find('_mutex', private_idx)
        self.assertNotEqual(mutex_idx, -1,
                            "_mutex should be declared after private:")


class TestBufferCppMutexInit(unittest.TestCase):
    def setUp(self):
        self.source = read_file(BUFFER_CPP)

    def test_constructor_initializes_mutex_nullptr(self):
        """ReadingBuffer constructor must initialize _mutex to nullptr (in init list or body)"""
        # The init list is BEFORE the opening { so we look at a broader region
        # that includes the function signature + init list + body.
        pattern = re.compile(
            r'ReadingBuffer::ReadingBuffer\b[^;]*?\{[^}]*\}',
            re.DOTALL
        )
        m = pattern.search(self.source)
        self.assertIsNotNone(m,
                             "Could not find ReadingBuffer::ReadingBuffer() in buffer.cpp")
        ctor_region = m.group(0)
        self.assertIn('_mutex', ctor_region,
                      "Constructor must initialize _mutex (in init list or body)")
        self.assertIn('nullptr', ctor_region,
                      "Constructor must initialize _mutex to nullptr")

    def test_begin_creates_mutex(self):
        """begin() must call xSemaphoreCreateMutex()"""
        begin_body = extract_function_body(self.source, 'ReadingBuffer::begin')
        self.assertIsNotNone(begin_body,
                             "Could not find ReadingBuffer::begin() in buffer.cpp")
        self.assertIn('xSemaphoreCreateMutex', begin_body,
                      "begin() must create the mutex with xSemaphoreCreateMutex()")

    def test_freertos_semaphore_header_included(self):
        """buffer.cpp must include FreeRTOS semaphore header"""
        self.assertTrue(
            'semphr.h' in self.source or 'FreeRTOS.h' in self.source,
            "buffer.cpp should include FreeRTOS semaphore headers"
        )


class TestMutexInGuardedMethods(unittest.TestCase):
    """appendReading, getUnuploaded, markUploaded, pruneUploaded must take/give mutex"""

    GUARDED_METHODS = [
        'ReadingBuffer::appendReading',
        'ReadingBuffer::getUnuploaded',
        'ReadingBuffer::markUploaded',
        'ReadingBuffer::pruneUploaded',
    ]

    def setUp(self):
        self.source = read_file(BUFFER_CPP)

    def _get_body(self, method):
        body = extract_function_body(self.source, method)
        self.assertIsNotNone(body, f"Could not find {method}() in buffer.cpp")
        return body

    def test_appendReading_takes_mutex(self):
        body = self._get_body('ReadingBuffer::appendReading')
        self.assertIn('xSemaphoreTake', body,
                      "appendReading() must call xSemaphoreTake")

    def test_appendReading_gives_mutex(self):
        body = self._get_body('ReadingBuffer::appendReading')
        self.assertIn('xSemaphoreGive', body,
                      "appendReading() must call xSemaphoreGive")

    def test_getUnuploaded_takes_mutex(self):
        body = self._get_body('ReadingBuffer::getUnuploaded')
        self.assertIn('xSemaphoreTake', body,
                      "getUnuploaded() must call xSemaphoreTake")

    def test_getUnuploaded_gives_mutex(self):
        body = self._get_body('ReadingBuffer::getUnuploaded')
        self.assertIn('xSemaphoreGive', body,
                      "getUnuploaded() must call xSemaphoreGive")

    def test_markUploaded_takes_mutex(self):
        body = self._get_body('ReadingBuffer::markUploaded')
        self.assertIn('xSemaphoreTake', body,
                      "markUploaded() must call xSemaphoreTake")

    def test_markUploaded_gives_mutex(self):
        body = self._get_body('ReadingBuffer::markUploaded')
        self.assertIn('xSemaphoreGive', body,
                      "markUploaded() must call xSemaphoreGive")

    def test_pruneUploaded_takes_mutex(self):
        body = self._get_body('ReadingBuffer::pruneUploaded')
        self.assertIn('xSemaphoreTake', body,
                      "pruneUploaded() must call xSemaphoreTake")

    def test_pruneUploaded_gives_mutex(self):
        body = self._get_body('ReadingBuffer::pruneUploaded')
        self.assertIn('xSemaphoreGive', body,
                      "pruneUploaded() must call xSemaphoreGive")


class TestNoDoubleLockinGetStatsAndSaveIndex(unittest.TestCase):
    """getStats() and _saveIndex() must NOT independently take the mutex"""

    def setUp(self):
        self.source = read_file(BUFFER_CPP)

    def test_getStats_does_not_take_mutex(self):
        """getStats() is called while mutex is held; must not double-lock"""
        body = extract_function_body(self.source, 'ReadingBuffer::getStats')
        self.assertIsNotNone(body,
                             "Could not find ReadingBuffer::getStats() in buffer.cpp")
        self.assertNotIn('xSemaphoreTake', body,
                         "getStats() must NOT call xSemaphoreTake (would double-lock)")

    def test_saveIndex_does_not_take_mutex(self):
        """_saveIndex() is called while mutex is held; must not double-lock"""
        body = extract_function_body(self.source, 'ReadingBuffer::_saveIndex')
        self.assertIsNotNone(body,
                             "Could not find ReadingBuffer::_saveIndex() in buffer.cpp")
        self.assertNotIn('xSemaphoreTake', body,
                         "_saveIndex() must NOT call xSemaphoreTake (would double-lock)")


if __name__ == '__main__':
    print("=== RL-003 Buffer Mutex Tests ===")
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestBufferHMutexMember))
    suite.addTests(loader.loadTestsFromTestCase(TestBufferCppMutexInit))
    suite.addTests(loader.loadTestsFromTestCase(TestMutexInGuardedMethods))
    suite.addTests(loader.loadTestsFromTestCase(TestNoDoubleLockinGetStatsAndSaveIndex))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)

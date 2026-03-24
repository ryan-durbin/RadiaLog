#!/usr/bin/env python3
"""
US-003: Tests for ReadingBuffer::begin() - LittleFS init and index file loading.
Validates source code contains the required LittleFS patterns and begin() logic.
Run with: python3 firmware/test/test_buffer_init.py
"""

import os
import re
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FIRMWARE_DIR = os.path.join(REPO_ROOT, 'firmware')
SRC_DIR = os.path.join(FIRMWARE_DIR, 'src')

PASS = 0
FAIL = 0


def ok(msg):
    global PASS
    print(f"  ✅ {msg}")
    PASS += 1


def fail(msg):
    global FAIL
    print(f"  ❌ {msg}")
    FAIL += 1


def check(condition, pass_msg, fail_msg):
    if condition:
        ok(pass_msg)
    else:
        fail(fail_msg)


header_path = os.path.join(SRC_DIR, 'buffer.h')
impl_path   = os.path.join(SRC_DIR, 'buffer.cpp')

# ---------------------------------------------------------------------------
# Load source files
# ---------------------------------------------------------------------------
header = ''
impl   = ''

if os.path.isfile(header_path):
    with open(header_path) as f:
        header = f.read()

if os.path.isfile(impl_path):
    with open(impl_path) as f:
        impl = f.read()


# ---------------------------------------------------------------------------
# Test 1: File existence
# ---------------------------------------------------------------------------
print("\n=== Test 1: File existence ===")
check(os.path.isfile(header_path), 'buffer.h exists', 'buffer.h NOT found')
check(os.path.isfile(impl_path),   'buffer.cpp exists', 'buffer.cpp NOT found')


# ---------------------------------------------------------------------------
# Test 2: LittleFS.begin(true) is called in begin()
# ---------------------------------------------------------------------------
print("\n=== Test 2: LittleFS.begin(true) called ===")
check(
    re.search(r'LittleFS\.begin\s*\(\s*true\s*\)', impl),
    'LittleFS.begin(true) called in buffer.cpp',
    'LittleFS.begin(true) NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 3: LittleFS.h is included
# ---------------------------------------------------------------------------
print("\n=== Test 3: LittleFS.h included ===")
check(
    re.search(r'#include\s+[<"]LittleFS\.h[">]', impl),
    '#include <LittleFS.h> found in buffer.cpp',
    '#include <LittleFS.h> NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 4: /readings.bin path referenced
# ---------------------------------------------------------------------------
print("\n=== Test 4: /readings.bin file path ===")
check(
    '/readings.bin' in impl,
    '/readings.bin path referenced in buffer.cpp',
    '/readings.bin NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 5: /readings_idx.bin path referenced
# ---------------------------------------------------------------------------
print("\n=== Test 5: /readings_idx.bin file path ===")
check(
    '/readings_idx.bin' in impl,
    '/readings_idx.bin path referenced in buffer.cpp',
    '/readings_idx.bin NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 6: begin() creates /readings.bin if absent (LittleFS.exists check)
# ---------------------------------------------------------------------------
print("\n=== Test 6: begin() creates /readings.bin if absent ===")
check(
    re.search(r'LittleFS\.exists\s*\(', impl),
    'LittleFS.exists() called (checking for file absence)',
    'LittleFS.exists() NOT found — file creation may be missing'
)
# Should open readings file in write mode to create it
check(
    re.search(r'LittleFS\.open\s*\([^,]+READINGS_FILE[^,]*,\s*"w"\)', impl) or
    re.search(r'LittleFS\.open\s*\(\s*READINGS_FILE\s*,\s*"w"\s*\)', impl) or
    re.search(r'open\s*\([^,]+readings\.bin[^,]*,\s*"w"\)', impl) or
    (re.search(r'READINGS_FILE', impl) and re.search(r'"w"', impl)),
    '/readings.bin opened in write mode to create it',
    '/readings.bin write-create pattern NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 7: Index file read — uint32_t[3] pattern
# ---------------------------------------------------------------------------
print("\n=== Test 7: Index file stores uint32_t[3] ===")
# INDEX_SIZE or sizeof(uint32_t) * 3
check(
    re.search(r'uint32_t\s+buf\s*\[3\]', impl) or
    re.search(r'sizeof\s*\(\s*uint32_t\s*\)\s*\*\s*3', impl),
    'uint32_t[3] buffer used for index read/write',
    'uint32_t[3] index buffer NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 8: Corrupt/undersized index falls back to zero
# ---------------------------------------------------------------------------
print("\n=== Test 8: Corrupt index falls back to zeros ===")
# Must have a check that bytesRead < INDEX_SIZE (or equivalent) → zero init
check(
    re.search(r'bytesRead\s*<\s*INDEX_SIZE', impl) or
    re.search(r'bytes_read\s*<', impl) or
    (re.search(r'<\s*INDEX_SIZE', impl) and re.search(r'_depth\s*=\s*0', impl)),
    'Corrupt/short index falls back to zero-initialized stats',
    'No undersized-index fallback found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 9: Missing index → zero init (no file → defaults)
# ---------------------------------------------------------------------------
print("\n=== Test 9: Missing index file → zero-initialized stats ===")
# When index does not exist, _depth/_lifetimeLogged/_lifetimeUploaded = 0
check(
    re.search(r'_depth\s*=\s*0', impl),
    '_depth initialized to 0 when index absent',
    '_depth = 0 NOT found in buffer.cpp'
)
check(
    re.search(r'_lifetimeLogged\s*=\s*0', impl),
    '_lifetimeLogged initialized to 0 when index absent',
    '_lifetimeLogged = 0 NOT found in buffer.cpp'
)
check(
    re.search(r'_lifetimeUploaded\s*=\s*0', impl),
    '_lifetimeUploaded initialized to 0 when index absent',
    '_lifetimeUploaded = 0 NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 10: Existing index → stats loaded from buf[0], buf[1], buf[2]
# ---------------------------------------------------------------------------
print("\n=== Test 10: Existing index → stats loaded from buffer ===")
check(
    re.search(r'_depth\s*=\s*buf\s*\[\s*0\s*\]', impl),
    '_depth assigned from buf[0]',
    '_depth = buf[0] NOT found in buffer.cpp'
)
check(
    re.search(r'_lifetimeLogged\s*=\s*buf\s*\[\s*1\s*\]', impl),
    '_lifetimeLogged assigned from buf[1]',
    '_lifetimeLogged = buf[1] NOT found in buffer.cpp'
)
check(
    re.search(r'_lifetimeUploaded\s*=\s*buf\s*\[\s*2\s*\]', impl),
    '_lifetimeUploaded assigned from buf[2]',
    '_lifetimeUploaded = buf[2] NOT found in buffer.cpp'
)


# ---------------------------------------------------------------------------
# Test 11: _saveIndex() persists index (write-through)
# ---------------------------------------------------------------------------
print("\n=== Test 11: _saveIndex() write-through persistence ===")
check(
    re.search(r'_saveIndex\s*\(\s*\)', impl),
    '_saveIndex() called in begin() for write-through',
    '_saveIndex() NOT called in buffer.cpp'
)
check(
    re.search(r'bool\s+ReadingBuffer::_saveIndex\s*\(', impl),
    'ReadingBuffer::_saveIndex() implemented in buffer.cpp',
    'ReadingBuffer::_saveIndex() NOT found in buffer.cpp'
)
check(
    re.search(r'bool\s+_saveIndex\s*\(\s*\)\s*;', header),
    '_saveIndex() declared in buffer.h',
    '_saveIndex() NOT declared in buffer.h'
)


# ---------------------------------------------------------------------------
# Test 12: begin() returns bool
# ---------------------------------------------------------------------------
print("\n=== Test 12: begin() return type is bool ===")
check(
    re.search(r'bool\s+ReadingBuffer::begin\s*\(', impl),
    'ReadingBuffer::begin() returns bool',
    'ReadingBuffer::begin() bool return type NOT found'
)
check(
    re.search(r'bool\s+begin\s*\(\s*\)\s*;', header),
    'begin() declared as bool in buffer.h',
    'begin() bool declaration NOT found in buffer.h'
)


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All tests passed! ✅")
    sys.exit(0)

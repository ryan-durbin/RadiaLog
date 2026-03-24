#!/usr/bin/env python3
"""
US-002: Tests for BufferStats struct and ReadingBuffer class skeleton in buffer.h/cpp
Validates that the header defines the required struct, class, and method signatures.
Run with: python3 firmware/test/test_buffer_stats.py
"""

import os
import sys
import re

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


# ---------------------------------------------------------------------------
# Test 1: buffer.h and buffer.cpp exist
# ---------------------------------------------------------------------------
print("\n=== Test 1: File existence ===")

header_path = os.path.join(SRC_DIR, 'buffer.h')
impl_path   = os.path.join(SRC_DIR, 'buffer.cpp')

check(os.path.isfile(header_path), 'buffer.h exists', 'buffer.h NOT found')
check(os.path.isfile(impl_path),   'buffer.cpp exists', 'buffer.cpp NOT found')


# ---------------------------------------------------------------------------
# Test 2: BufferStats struct has required fields
# ---------------------------------------------------------------------------
print("\n=== Test 2: BufferStats struct fields ===")

if os.path.isfile(header_path):
    with open(header_path) as f:
        header = f.read()

    # Struct declaration
    check('struct BufferStats' in header,
          'struct BufferStats declared',
          'struct BufferStats NOT found in buffer.h')

    # Required fields with uint32_t type
    check(re.search(r'uint32_t\s+depth\s*;', header),
          'BufferStats::depth (uint32_t) found',
          'BufferStats::depth (uint32_t) NOT found')

    check(re.search(r'uint32_t\s+lifetimeLogged\s*;', header),
          'BufferStats::lifetimeLogged (uint32_t) found',
          'BufferStats::lifetimeLogged (uint32_t) NOT found')

    check(re.search(r'uint32_t\s+lifetimeUploaded\s*;', header),
          'BufferStats::lifetimeUploaded (uint32_t) found',
          'BufferStats::lifetimeUploaded (uint32_t) NOT found')


# ---------------------------------------------------------------------------
# Test 3: ReadingBuffer class has required methods
# ---------------------------------------------------------------------------
print("\n=== Test 3: ReadingBuffer class methods ===")

if os.path.isfile(header_path):
    check('class ReadingBuffer' in header,
          'class ReadingBuffer declared',
          'class ReadingBuffer NOT found in buffer.h')

    check(re.search(r'\bbegin\s*\(', header),
          'begin() declared',
          'begin() NOT found in ReadingBuffer')

    check(re.search(r'\bgetStats\s*\(', header),
          'getStats() declared',
          'getStats() NOT found in ReadingBuffer')

    check(re.search(r'\bappendReading\s*\(', header),
          'appendReading() declared',
          'appendReading() NOT found in ReadingBuffer')

    check(re.search(r'\bgetUnuploaded\s*\(', header),
          'getUnuploaded() declared',
          'getUnuploaded() NOT found in ReadingBuffer')

    check(re.search(r'\bmarkUploaded\s*\(', header),
          'markUploaded() declared',
          'markUploaded() NOT found in ReadingBuffer')

    check(re.search(r'\bpruneUploaded\s*\(', header),
          'pruneUploaded() declared',
          'pruneUploaded() NOT found in ReadingBuffer')


# ---------------------------------------------------------------------------
# Test 4: ReadingBuffer private members
# ---------------------------------------------------------------------------
print("\n=== Test 4: ReadingBuffer private members ===")

if os.path.isfile(header_path):
    check(re.search(r'uint32_t\s+_depth\s*;', header),
          '_depth (uint32_t) private member found',
          '_depth (uint32_t) NOT found in ReadingBuffer')

    check(re.search(r'uint32_t\s+_lifetimeLogged\s*;', header),
          '_lifetimeLogged (uint32_t) private member found',
          '_lifetimeLogged (uint32_t) NOT found in ReadingBuffer')

    check(re.search(r'uint32_t\s+_lifetimeUploaded\s*;', header),
          '_lifetimeUploaded (uint32_t) private member found',
          '_lifetimeUploaded (uint32_t) NOT found in ReadingBuffer')


# ---------------------------------------------------------------------------
# Test 5: buffer.h includes config.h
# ---------------------------------------------------------------------------
print("\n=== Test 5: config.h integration ===")

if os.path.isfile(header_path):
    check(re.search(r'#include\s+"config\.h"', header),
          'buffer.h includes config.h',
          'buffer.h does NOT include config.h')


# ---------------------------------------------------------------------------
# Test 6: buffer.cpp implements getStats() returning BufferStats
# ---------------------------------------------------------------------------
print("\n=== Test 6: buffer.cpp getStats() implementation ===")

if os.path.isfile(impl_path):
    with open(impl_path) as f:
        impl = f.read()

    check('BufferStats' in impl,
          'buffer.cpp references BufferStats',
          'buffer.cpp does NOT reference BufferStats')

    check(re.search(r'ReadingBuffer::getStats\s*\(', impl),
          'ReadingBuffer::getStats() implemented in buffer.cpp',
          'ReadingBuffer::getStats() NOT found in buffer.cpp')

    check(re.search(r'ReadingBuffer::begin\s*\(', impl),
          'ReadingBuffer::begin() implemented in buffer.cpp',
          'ReadingBuffer::begin() NOT found in buffer.cpp')

    check(re.search(r'_depth\s*=\s*0', impl),
          'begin() initializes _depth to 0',
          'begin() does NOT initialize _depth to 0')

    check(re.search(r'_lifetimeLogged\s*=\s*0', impl),
          'begin() initializes _lifetimeLogged to 0',
          'begin() does NOT initialize _lifetimeLogged to 0')

    check(re.search(r'_lifetimeUploaded\s*=\s*0', impl),
          'begin() initializes _lifetimeUploaded to 0',
          'begin() does NOT initialize _lifetimeUploaded to 0')

    # getStats() must return a BufferStats with the correct fields assigned
    check(re.search(r'stats\.depth\s*=\s*_depth', impl),
          'getStats() assigns stats.depth = _depth',
          'getStats() does NOT assign stats.depth')

    check(re.search(r'stats\.lifetimeLogged\s*=\s*_lifetimeLogged', impl),
          'getStats() assigns stats.lifetimeLogged = _lifetimeLogged',
          'getStats() does NOT assign stats.lifetimeLogged')

    check(re.search(r'stats\.lifetimeUploaded\s*=\s*_lifetimeUploaded', impl),
          'getStats() assigns stats.lifetimeUploaded = _lifetimeUploaded',
          'getStats() does NOT assign stats.lifetimeUploaded')


# ---------------------------------------------------------------------------
# Test 7: READING_BINARY_SIZE constant
# ---------------------------------------------------------------------------
print("\n=== Test 7: READING_BINARY_SIZE constant ===")

if os.path.isfile(header_path):
    check(re.search(r'READING_BINARY_SIZE\s*=\s*34', header),
          'READING_BINARY_SIZE = 34 defined',
          'READING_BINARY_SIZE = 34 NOT found in buffer.h')


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All tests passed! ✅")
    sys.exit(0)

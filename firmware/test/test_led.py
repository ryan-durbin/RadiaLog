#!/usr/bin/env python3
"""
US-001: Tests for LED status pattern module (led.h / led.cpp)
Validates that led.h and led.cpp exist and meet all acceptance criteria.
Run with: python3 firmware/test/test_led.py
"""

import os
import sys
import re

# Paths
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FIRMWARE_DIR = os.path.join(REPO_ROOT, 'firmware')
SRC_DIR = os.path.join(FIRMWARE_DIR, 'src')
LED_H = os.path.join(SRC_DIR, 'led.h')
LED_CPP = os.path.join(SRC_DIR, 'led.cpp')

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
# Test 1: Files exist
# ---------------------------------------------------------------------------
print("\n=== Test 1: File existence ===")
check(os.path.isfile(LED_H), 'led.h exists', 'led.h NOT found')
check(os.path.isfile(LED_CPP), 'led.cpp exists', 'led.cpp NOT found')

if not os.path.isfile(LED_H) or not os.path.isfile(LED_CPP):
    print("\n⛔ Cannot continue — source files missing.")
    sys.exit(1)

with open(LED_H) as f:
    header = f.read()
with open(LED_CPP) as f:
    impl = f.read()


# ---------------------------------------------------------------------------
# Test 2: Header guard
# ---------------------------------------------------------------------------
print("\n=== Test 2: Header guard ===")
check('#pragma once' in header, 'led.h has #pragma once', 'led.h MISSING #pragma once')


# ---------------------------------------------------------------------------
# Test 3: LedPattern enum defines all 5 patterns
# ---------------------------------------------------------------------------
print("\n=== Test 3: LedPattern enum ===")
patterns = ['SOLID', 'SLOW_BLINK', 'FAST_BLINK', 'DOUBLE_FLASH', 'OFF']
enum_present = 'LedPattern' in header
check(enum_present, 'LedPattern enum declared', 'LedPattern enum NOT found in led.h')

for p in patterns:
    check(p in header, f'Pattern {p} defined', f'Pattern {p} NOT found in led.h')


# ---------------------------------------------------------------------------
# Test 4: Led class methods declared in header
# ---------------------------------------------------------------------------
print("\n=== Test 4: Class method declarations ===")
check(re.search(r'\bsetPattern\s*\(', header),
      'setPattern() declared in led.h',
      'setPattern() NOT found in led.h')

check(re.search(r'\bupdate\s*\(\s*\)', header),
      'update() declared in led.h',
      'update() NOT found in led.h')

check(re.search(r'\bbegin\s*\(\s*\)', header),
      'begin() declared in led.h',
      'begin() NOT found in led.h')


# ---------------------------------------------------------------------------
# Test 5: No delay() calls in led.cpp
# ---------------------------------------------------------------------------
print("\n=== Test 5: No delay() in led.cpp ===")
delay_calls = re.findall(r'\bdelay\s*\(', impl)
check(len(delay_calls) == 0,
      'No delay() calls in led.cpp',
      f'Found {len(delay_calls)} delay() call(s) in led.cpp — must be non-blocking')


# ---------------------------------------------------------------------------
# Test 6: Uses millis() for timing
# ---------------------------------------------------------------------------
print("\n=== Test 6: millis() usage ===")
check('millis()' in impl,
      'led.cpp uses millis() for timing',
      'led.cpp does NOT use millis() — timing must be millis-based')


# ---------------------------------------------------------------------------
# Test 7: Includes config.h and references LED_PIN
# ---------------------------------------------------------------------------
print("\n=== Test 7: config.h and LED_PIN ===")
check('#include "config.h"' in header or '#include "config.h"' in impl,
      'config.h included',
      'config.h NOT included in led.h or led.cpp')

check('LED_PIN' in header or 'LED_PIN' in impl,
      'LED_PIN referenced',
      'LED_PIN NOT referenced in led files')


# ---------------------------------------------------------------------------
# Test 8: Timing constants — SLOW_BLINK ~1000ms, FAST_BLINK ~100-200ms
# ---------------------------------------------------------------------------
print("\n=== Test 8: Timing constants ===")

# Look for SLOW_BLINK period ~1000ms
slow_match = re.search(r'SLOW_BLINK[_A-Z]*\s*=\s*(\d+)', impl)
if slow_match:
    slow_val = int(slow_match.group(1))
    check(800 <= slow_val <= 1200,
          f'SLOW_BLINK period ~1000ms (got {slow_val}ms)',
          f'SLOW_BLINK period out of range: {slow_val}ms (expected ~1000ms)')
else:
    # Also accept literal 1000 near slow blink context
    check(re.search(r'1000', impl),
          'SLOW_BLINK 1000ms period value present',
          'SLOW_BLINK 1000ms period NOT found in led.cpp')

# Look for FAST_BLINK period ~100-200ms
fast_match = re.search(r'FAST_BLINK[_A-Z]*\s*=\s*(\d+)', impl)
if fast_match:
    fast_val = int(fast_match.group(1))
    check(50 <= fast_val <= 200,
          f'FAST_BLINK period ~100-200ms (got {fast_val}ms)',
          f'FAST_BLINK period out of range: {fast_val}ms (expected 100-200ms)')
else:
    check(re.search(r'\b(100|150|200)\b', impl),
          'FAST_BLINK short period value (100-200ms) present',
          'FAST_BLINK short period NOT found in led.cpp')


# ---------------------------------------------------------------------------
# Test 9: DOUBLE_FLASH logic present
# ---------------------------------------------------------------------------
print("\n=== Test 9: DOUBLE_FLASH implementation ===")
check('DOUBLE_FLASH' in impl,
      'DOUBLE_FLASH case handled in led.cpp',
      'DOUBLE_FLASH NOT handled in led.cpp')

# Should have some concept of flash count / two flashes
check(re.search(r'(flash|Flash|FLASH).*(count|Count|COUNT|[>=]=\s*2)', impl, re.DOTALL) or
      re.search(r'(count|Count|COUNT).*(flash|Flash|FLASH)', impl, re.DOTALL) or
      re.search(r'_flashCount', impl),
      'DOUBLE_FLASH tracks flash count',
      'DOUBLE_FLASH does NOT appear to count flashes')


# ---------------------------------------------------------------------------
# Test 10: setPattern takes LedPattern argument
# ---------------------------------------------------------------------------
print("\n=== Test 10: setPattern signature ===")
check(re.search(r'setPattern\s*\(\s*LedPattern\s+\w+\s*\)', header),
      'setPattern(LedPattern) properly typed',
      'setPattern(LedPattern) signature NOT found in header')


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All LED tests passed! ✅")
    sys.exit(0)

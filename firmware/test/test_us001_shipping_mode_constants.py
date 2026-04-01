#!/usr/bin/env python3
"""
US-001: Tests for shipping mode constants and BOOT_BUTTON_PIN in config.h
Validates that config.h defines all required constants for shipping mode.
Run with: python3 firmware/test/test_us001_shipping_mode_constants.py
"""

import os
import re
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CONFIG_H = os.path.join(REPO_ROOT, 'firmware', 'src', 'config.h')

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
# Load config.h
# ---------------------------------------------------------------------------
print("\n=== Test 0: config.h exists ===")
if not os.path.isfile(CONFIG_H):
    fail(f"config.h NOT found at {CONFIG_H}")
    print("\n⛔ Cannot continue — config.h missing.")
    sys.exit(1)
ok(f"config.h found at {CONFIG_H}")

with open(CONFIG_H) as f:
    content = f.read()


# ---------------------------------------------------------------------------
# Test 1: BOOT_BUTTON_PIN = 0
# ---------------------------------------------------------------------------
print("\n=== Test 1: BOOT_BUTTON_PIN defined as 0 ===")
m = re.search(r'#define\s+BOOT_BUTTON_PIN\s+(\d+)', content)
check(m is not None, 'BOOT_BUTTON_PIN is defined', 'BOOT_BUTTON_PIN NOT found in config.h')
if m:
    check(int(m.group(1)) == 0,
          f'BOOT_BUTTON_PIN == 0 (got {m.group(1)})',
          f'BOOT_BUTTON_PIN != 0 (got {m.group(1)})')


# ---------------------------------------------------------------------------
# Test 2: SHIPPING_MODE_HOLD_MS = 5000
# ---------------------------------------------------------------------------
print("\n=== Test 2: SHIPPING_MODE_HOLD_MS defined as 5000 ===")
m = re.search(r'#define\s+SHIPPING_MODE_HOLD_MS\s+(\d+)', content)
check(m is not None, 'SHIPPING_MODE_HOLD_MS is defined', 'SHIPPING_MODE_HOLD_MS NOT found in config.h')
if m:
    check(int(m.group(1)) == 5000,
          f'SHIPPING_MODE_HOLD_MS == 5000 (got {m.group(1)})',
          f'SHIPPING_MODE_HOLD_MS != 5000 (got {m.group(1)})')


# ---------------------------------------------------------------------------
# Test 3: SHIPPING_MODE_BLINK_COUNT = 3
# ---------------------------------------------------------------------------
print("\n=== Test 3: SHIPPING_MODE_BLINK_COUNT defined as 3 ===")
m = re.search(r'#define\s+SHIPPING_MODE_BLINK_COUNT\s+(\d+)', content)
check(m is not None, 'SHIPPING_MODE_BLINK_COUNT is defined', 'SHIPPING_MODE_BLINK_COUNT NOT found in config.h')
if m:
    check(int(m.group(1)) == 3,
          f'SHIPPING_MODE_BLINK_COUNT == 3 (got {m.group(1)})',
          f'SHIPPING_MODE_BLINK_COUNT != 3 (got {m.group(1)})')


# ---------------------------------------------------------------------------
# Test 4: SHIPPING_MODE_BLINK_MS = 200
# ---------------------------------------------------------------------------
print("\n=== Test 4: SHIPPING_MODE_BLINK_MS defined as 200 ===")
m = re.search(r'#define\s+SHIPPING_MODE_BLINK_MS\s+(\d+)', content)
check(m is not None, 'SHIPPING_MODE_BLINK_MS is defined', 'SHIPPING_MODE_BLINK_MS NOT found in config.h')
if m:
    check(int(m.group(1)) == 200,
          f'SHIPPING_MODE_BLINK_MS == 200 (got {m.group(1)})',
          f'SHIPPING_MODE_BLINK_MS != 200 (got {m.group(1)})')


# ---------------------------------------------------------------------------
# Test 5: Constants appear in common settings section
# ---------------------------------------------------------------------------
print("\n=== Test 5: Constants in common settings section ===")
# Find position of "Common settings" header and check #define lines appear after it
common_pos = content.find('Common settings (all boards)')
define_boot_pos = content.find('#define BOOT_BUTTON_PIN')
define_shipping_pos = content.find('#define SHIPPING_MODE_HOLD_MS')
check(common_pos != -1 and define_boot_pos > common_pos,
      'BOOT_BUTTON_PIN is in common settings section',
      'BOOT_BUTTON_PIN NOT in common settings section')
check(common_pos != -1 and define_shipping_pos > common_pos,
      'SHIPPING_MODE_HOLD_MS is in common settings section',
      'SHIPPING_MODE_HOLD_MS NOT in common settings section')


# ---------------------------------------------------------------------------
# Test 6: T-Display S3 BUTTON2_PIN=0 has shared comment
# ---------------------------------------------------------------------------
print("\n=== Test 6: GPIO0 shared note on T-Display S3 BUTTON2_PIN ===")
# Match from #elif BOARD_TDISPLAY_S3 (not the comment header) to the next #elif/#else/#error
tdisplay_section = re.search(
    r'#elif\s+defined\(BOARD_TDISPLAY_S3\).*?(?=#elif|#else|#error)',
    content, re.DOTALL
)
if tdisplay_section:
    section_text = tdisplay_section.group(0)
    check('BUTTON2_PIN' in section_text and ('shared' in section_text or 'BOOT_BUTTON_PIN' in section_text),
          'T-Display S3 BUTTON2_PIN=0 has shared/BOOT_BUTTON_PIN comment',
          'T-Display S3 BUTTON2_PIN=0 missing shared comment')
else:
    fail('Could not locate #elif BOARD_TDISPLAY_S3 section in config.h')


# ---------------------------------------------------------------------------
# Test 7: AMOLED BUTTON2_PIN=0 has shared comment
# ---------------------------------------------------------------------------
print("\n=== Test 7: GPIO0 shared note on AMOLED BUTTON2_PIN ===")
amoled_section = re.search(
    r'#elif\s+defined\(BOARD_TDISPLAY_S3_AMOLED\).*?(?=#elif|#else|#error)',
    content, re.DOTALL
)
if amoled_section:
    section_text = amoled_section.group(0)
    check('BUTTON2_PIN' in section_text and ('shared' in section_text or 'BOOT_BUTTON_PIN' in section_text),
          'AMOLED BUTTON2_PIN=0 has shared/BOOT_BUTTON_PIN comment',
          'AMOLED BUTTON2_PIN=0 missing shared comment')
else:
    fail('Could not locate #elif BOARD_TDISPLAY_S3_AMOLED section in config.h')


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All US-001 shipping mode constant tests passed! ✅")
    sys.exit(0)

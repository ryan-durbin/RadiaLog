#!/usr/bin/env python3
"""
Tests for US-003: ShippingMode module with GPIO0 long-press detection.

Validates the source files (shipping_mode.h / shipping_mode.cpp) against
the acceptance criteria by parsing the code.
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(REPO, "src")

passed = 0
failed = 0

def check(name, condition, detail=""):
    global passed, failed
    if condition:
        print(f"  ✅ {name}")
        passed += 1
    else:
        print(f"  ❌ {name}" + (f" — {detail}" if detail else ""))
        failed += 1

# ---------------------------------------------------------------------------
# Load source files
# ---------------------------------------------------------------------------
header_path = os.path.join(SRC, "shipping_mode.h")
impl_path   = os.path.join(SRC, "shipping_mode.cpp")

print("=== US-003: ShippingMode module ===\n")

# AC-1: shipping_mode.h exists
check("AC-1: shipping_mode.h exists", os.path.isfile(header_path))

# AC-2: shipping_mode.cpp exists
check("AC-2: shipping_mode.cpp exists", os.path.isfile(impl_path))

if not os.path.isfile(header_path) or not os.path.isfile(impl_path):
    print(f"\n{'passed'}: {passed}, {'failed'}: {failed}")
    sys.exit(1)

header = open(header_path).read()
impl   = open(impl_path).read()

# ---------------------------------------------------------------------------
# AC-1 / AC-2: Class declaration
# ---------------------------------------------------------------------------
check("Class ShippingMode declared in header",
      "class ShippingMode" in header)

check("begin() declared",
      re.search(r'void\s+begin\s*\(\s*\)', header) is not None)

check("update() declared",
      re.search(r'void\s+update\s*\(\s*\)', header) is not None)

check("shouldEnterSleep() declared",
      re.search(r'bool\s+shouldEnterSleep\s*\(\s*\)', header) is not None)

check("reset() declared",
      re.search(r'void\s+reset\s*\(\s*\)', header) is not None)

# ---------------------------------------------------------------------------
# AC-3: begin() configures BOOT_BUTTON_PIN as INPUT_PULLUP
# ---------------------------------------------------------------------------
check("AC-3: begin() uses pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP)",
      re.search(r'pinMode\s*\(\s*BOOT_BUTTON_PIN\s*,\s*INPUT_PULLUP\s*\)', impl) is not None)

# ---------------------------------------------------------------------------
# AC-4: update() uses millis()-based timing, no delay()
# ---------------------------------------------------------------------------
check("AC-4a: update() calls millis()",
      "millis()" in impl)

check("AC-4b: No delay() calls in implementation",
      "delay(" not in impl)

# ---------------------------------------------------------------------------
# AC-5: shouldEnterSleep() returns true after SHIPPING_MODE_HOLD_MS
# ---------------------------------------------------------------------------
check("AC-5: Uses SHIPPING_MODE_HOLD_MS for threshold",
      "SHIPPING_MODE_HOLD_MS" in impl)

check("AC-5: _sleepRequested flag set when threshold reached",
      re.search(r'_sleepRequested\s*=\s*true', impl) is not None)

# ---------------------------------------------------------------------------
# AC-6: Button release resets the timer
# ---------------------------------------------------------------------------
check("AC-6: _pressStartMs reset on button release",
      re.search(r'_pressStartMs\s*=\s*0', impl) is not None)

check("AC-6: _buttonDown set to false on release",
      re.search(r'_buttonDown\s*=\s*false', impl) is not None)

# ---------------------------------------------------------------------------
# Debouncing
# ---------------------------------------------------------------------------
check("Debounce constant defined (50ms)",
      re.search(r'DEBOUNCE_MS\s*=\s*50', header) is not None or
      re.search(r'DEBOUNCE_MS\s*=\s*50', impl) is not None)

# ---------------------------------------------------------------------------
# Non-blocking: no delay() in header or impl
# ---------------------------------------------------------------------------
check("No delay() calls in header (comments OK)",
      re.search(r'^\s*[^/]*\bdelay\s*\(', header, re.MULTILINE) is None)

# ---------------------------------------------------------------------------
# Documentation: shared GPIO0 note
# ---------------------------------------------------------------------------
check("Header documents GPIO0 sharing with BUTTON2_PIN",
      "BUTTON2_PIN" in header and "GPIO0" in header)

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
check("Header includes config.h",
      '#include "config.h"' in header)

check("Impl includes shipping_mode.h",
      '#include "shipping_mode.h"' in impl)

# ---------------------------------------------------------------------------
# reset() clears flag
# ---------------------------------------------------------------------------
check("reset() clears _sleepRequested",
      re.search(r'void\s+ShippingMode::reset\s*\(\s*\)', impl) is not None and
      "_sleepRequested = false" in impl)

# ---------------------------------------------------------------------------
# digitalRead used for polling
# ---------------------------------------------------------------------------
check("Uses digitalRead(BOOT_BUTTON_PIN) for polling",
      re.search(r'digitalRead\s*\(\s*BOOT_BUTTON_PIN\s*\)', impl) is not None)

# ---------------------------------------------------------------------------
print(f"\n{'='*40}")
print(f"Passed: {passed}  Failed: {failed}")
if failed > 0:
    sys.exit(1)
else:
    print("All US-003 tests passed!")
    sys.exit(0)

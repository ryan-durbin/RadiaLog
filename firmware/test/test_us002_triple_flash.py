#!/usr/bin/env python3
"""Tests for US-002: TRIPLE_FLASH LED pattern for shipping mode confirmation."""

import re
import sys
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LED_H = os.path.join(REPO, "src", "led.h")
LED_CPP = os.path.join(REPO, "src", "led.cpp")
CONFIG_H = os.path.join(REPO, "src", "config.h")

failures = 0

def check(name, condition):
    global failures
    if condition:
        print(f"  ✅ {name}")
    else:
        print(f"  ❌ {name}")
        failures += 1

# Read files
with open(LED_H) as f:
    led_h = f.read()
with open(LED_CPP) as f:
    led_cpp = f.read()
with open(CONFIG_H) as f:
    config_h = f.read()

print("=== US-002: TRIPLE_FLASH LED Pattern Tests ===\n")

# AC1: TRIPLE_FLASH is a valid value in the LedPattern enum
print("AC1: TRIPLE_FLASH in LedPattern enum")
check("TRIPLE_FLASH declared in enum", "TRIPLE_FLASH" in led_h)
# Verify it's inside the enum class LedPattern block
enum_match = re.search(r'enum class LedPattern\s*\{([^}]+)\}', led_h, re.DOTALL)
check("LedPattern enum exists", enum_match is not None)
if enum_match:
    enum_body = enum_match.group(1)
    check("TRIPLE_FLASH is inside LedPattern enum", "TRIPLE_FLASH" in enum_body)
    # Verify it appears before OFF
    triple_pos = enum_body.find("TRIPLE_FLASH")
    off_pos = enum_body.find("OFF")
    check("TRIPLE_FLASH appears before OFF in enum", triple_pos < off_pos)

# AC2: Led::update() handles the TRIPLE_FLASH case
print("\nAC2: TRIPLE_FLASH case in Led::update()")
check("case LedPattern::TRIPLE_FLASH in led.cpp", "case LedPattern::TRIPLE_FLASH" in led_cpp)
check("Uses SHIPPING_MODE_BLINK_MS for timing", "SHIPPING_MODE_BLINK_MS" in led_cpp)
check("Uses SHIPPING_MODE_BLINK_COUNT for flash count", "SHIPPING_MODE_BLINK_COUNT" in led_cpp)

# AC3: isPatternComplete() method exists
print("\nAC3: isPatternComplete() method")
check("isPatternComplete declared in led.h", "isPatternComplete" in led_h)
check("isPatternComplete returns bool", "bool isPatternComplete()" in led_h)
check("_patternComplete member variable exists", "_patternComplete" in led_h)
# Check it's set to true after flashes complete
check("_patternComplete set to true in TRIPLE_FLASH handler", "_patternComplete = true" in led_cpp)

# AC4: No delay() calls in led.cpp
print("\nAC4: No delay() calls")
# Match delay( but not _lastToggle or other identifiers containing 'delay'
delay_calls = re.findall(r'\bdelay\s*\(', led_cpp)
check("No delay() calls in led.cpp", len(delay_calls) == 0)

# AC5: Existing patterns still present
print("\nAC5: Existing LED patterns unchanged")
for pattern in ["SOLID", "SLOW_BLINK", "FAST_BLINK", "DOUBLE_FLASH", "OFF"]:
    check(f"LedPattern::{pattern} still in enum", f"LedPattern::{pattern}" in led_h or pattern in enum_body)
    if pattern != "OFF":  # OFF doesn't have a complex case
        check(f"case LedPattern::{pattern} in update()", f"case LedPattern::{pattern}" in led_cpp)

# Additional checks
print("\nAdditional checks:")
check("_patternComplete initialized in constructor", "_patternComplete(false)" in led_cpp)
check("_patternComplete reset in setPattern()", "_patternComplete = false" in led_cpp)
check("Auto-transitions to OFF after completion", 
      "_pattern = LedPattern::OFF" in led_cpp)
check("config.h included in led.h", '#include "config.h"' in led_h)

# Verify SHIPPING_MODE constants exist in config.h
check("SHIPPING_MODE_BLINK_COUNT defined in config.h", "SHIPPING_MODE_BLINK_COUNT" in config_h)
check("SHIPPING_MODE_BLINK_MS defined in config.h", "SHIPPING_MODE_BLINK_MS" in config_h)

print()
if failures > 0:
    print(f"❌ {failures} test(s) FAILED")
    sys.exit(1)
else:
    print("✅ All tests passed")

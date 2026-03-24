#!/usr/bin/env python3
"""
US-001: Tests for PlatformIO project structure and config.h
Validates that all required files exist and contain required content.
Run with: python3 firmware/test/test_project_structure.py
"""

import os
import sys
import re
import configparser

# Root of the RadiaLog repo (two levels up from this file)
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
# Test 1: platformio.ini exists and is valid
# ---------------------------------------------------------------------------
print("\n=== Test 1: platformio.ini structure ===")

ini_path = os.path.join(FIRMWARE_DIR, 'platformio.ini')
check(os.path.isfile(ini_path), 'platformio.ini exists', 'platformio.ini NOT found')

if os.path.isfile(ini_path):
    parser = configparser.ConfigParser()
    parser.read(ini_path)

    # Board and framework
    env_section = 'env:seeed_xiao_esp32s3'
    check(env_section in parser.sections(),
          f'Section [{env_section}] found',
          f'Section [{env_section}] NOT found')

    if env_section in parser.sections():
        env = parser[env_section]
        check(env.get('board') == 'seeed_xiao_esp32s3',
              'board = seeed_xiao_esp32s3',
              f'board wrong: {env.get("board")}')

        check(env.get('framework') == 'arduino',
              'framework = arduino',
              f'framework wrong: {env.get("framework")}')

        build_flags = env.get('build_flags', '')
        check('-DARDUINO_USB_MODE=0' in build_flags,
              'build_flags includes -DARDUINO_USB_MODE=0',
              'build_flags MISSING -DARDUINO_USB_MODE=0')

        lib_deps = env.get('lib_deps', '')
        check('TinyGPSPlus' in lib_deps or 'tinygpsplus' in lib_deps.lower(),
              'lib_deps includes TinyGPSPlus',
              'lib_deps MISSING TinyGPSPlus')
        check('ArduinoJson' in lib_deps or 'arduinojson' in lib_deps.lower(),
              'lib_deps includes ArduinoJson',
              'lib_deps MISSING ArduinoJson')
        check('LittleFS' in lib_deps or 'littlefs' in lib_deps.lower(),
              'lib_deps includes LittleFS',
              'lib_deps MISSING LittleFS')
        check('ESPAsyncWebServer' in lib_deps or 'espasyncwebserver' in lib_deps.lower(),
              'lib_deps includes ESPAsyncWebServer',
              'lib_deps MISSING ESPAsyncWebServer')


# ---------------------------------------------------------------------------
# Test 2: config.h exists and defines required constants
# ---------------------------------------------------------------------------
print("\n=== Test 2: config.h required constants ===")

config_path = os.path.join(SRC_DIR, 'config.h')
check(os.path.isfile(config_path), 'config.h exists', 'config.h NOT found')

if os.path.isfile(config_path):
    with open(config_path) as f:
        content = f.read()

    required_defines = [
        'GPS_TX_PIN',
        'GPS_RX_PIN',
        'GPS_BAUD',
        'LED_PIN',
        'RADIACODE_VID',
        'RADIACODE_PID',
        'USB_WRITE_EP',
        'USB_READ_EP',
        'POLL_INTERVAL_MS',
    ]

    for symbol in required_defines:
        check(re.search(rf'#define\s+{symbol}\b', content),
              f'#define {symbol} found',
              f'#define {symbol} NOT found')

    # Check specific values
    check(re.search(r'#define\s+RADIACODE_VID\s+0x0483', content),
          'RADIACODE_VID = 0x0483',
          'RADIACODE_VID wrong value (expected 0x0483)')

    check(re.search(r'#define\s+RADIACODE_PID\s+0xF123', content),
          'RADIACODE_PID = 0xF123',
          'RADIACODE_PID wrong value (expected 0xF123)')

    check(re.search(r'#define\s+USB_WRITE_EP\s+0x01', content),
          'USB_WRITE_EP = 0x01',
          'USB_WRITE_EP wrong value (expected 0x01)')

    check(re.search(r'#define\s+USB_READ_EP\s+0x81', content),
          'USB_READ_EP = 0x81',
          'USB_READ_EP wrong value (expected 0x81)')

    check(re.search(r'#define\s+GPS_BAUD\s+9600', content),
          'GPS_BAUD = 9600',
          'GPS_BAUD wrong value (expected 9600)')

    check(re.search(r'#define\s+POLL_INTERVAL_MS\s+2000', content),
          'POLL_INTERVAL_MS = 2000',
          'POLL_INTERVAL_MS wrong value (expected 2000)')


# ---------------------------------------------------------------------------
# Test 3: main.cpp exists and has setup/loop
# ---------------------------------------------------------------------------
print("\n=== Test 3: main.cpp structure ===")

main_path = os.path.join(SRC_DIR, 'main.cpp')
check(os.path.isfile(main_path), 'main.cpp exists', 'main.cpp NOT found')

if os.path.isfile(main_path):
    with open(main_path) as f:
        content = f.read()

    check('#include "config.h"' in content or "#include 'config.h'" in content,
          'main.cpp includes config.h',
          'main.cpp does NOT include config.h')

    check(re.search(r'\bvoid\s+setup\s*\(\s*\)', content),
          'main.cpp has void setup()',
          'main.cpp MISSING void setup()')

    check(re.search(r'\bvoid\s+loop\s*\(\s*\)', content),
          'main.cpp has void loop()',
          'main.cpp MISSING void loop()')


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All tests passed! ✅")
    sys.exit(0)

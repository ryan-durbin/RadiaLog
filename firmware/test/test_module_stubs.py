#!/usr/bin/env python3
"""
US-001: Tests for all module stub headers and cpp files.
Validates that all required files exist with expected class/function signatures.
Run with: python3 firmware/test/test_module_stubs.py
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


def read_file(path):
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return f.read()


def check_file(path, label):
    exists = os.path.isfile(path)
    check(exists, f"{label} exists", f"{label} NOT found: {path}")
    return exists


def contains(content, pattern, flags=0):
    if content is None:
        return False
    return bool(re.search(pattern, content, flags))


# ---------------------------------------------------------------------------
# Test 1: reading.h — Reading struct
# ---------------------------------------------------------------------------
print("\n=== Test 1: src/reading.h — Reading struct ===")
reading_h = os.path.join(SRC_DIR, 'reading.h')
if check_file(reading_h, 'src/reading.h'):
    h = read_file(reading_h)
    check(contains(h, r'struct\s+Reading\s*\{'), 'struct Reading declared', 'struct Reading NOT found')
    fields = ['timestamp', 'lat', 'lon', 'dose_rate', 'count_rate',
              'altitude', 'speed_mph', 'speed_kph', 'heading',
              'accuracy', 'altitude_accuracy', 'gps_valid']
    for field in fields:
        check(contains(h, rf'\b{field}\b'), f'field {field} present', f'field {field} MISSING')
    check(contains(h, r'uint32_t\s+timestamp'), 'timestamp is uint32_t', 'timestamp type wrong')
    check(contains(h, r'double\s+lat'), 'lat is double', 'lat type wrong')
    check(contains(h, r'double\s+lon'), 'lon is double', 'lon type wrong')
    check(contains(h, r'bool\s+gps_valid'), 'gps_valid is bool', 'gps_valid type wrong')
    check(contains(h, r'float\s+dose_rate'), 'dose_rate is float', 'dose_rate type wrong')
    check(contains(h, r'float\s+count_rate'), 'count_rate is float', 'count_rate type wrong')


# ---------------------------------------------------------------------------
# Test 2: config_mgr.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 2: src/config_mgr.h/cpp ===")
cfg_h   = os.path.join(SRC_DIR, 'config_mgr.h')
cfg_cpp = os.path.join(SRC_DIR, 'config_mgr.cpp')
if check_file(cfg_h, 'src/config_mgr.h') and check_file(cfg_cpp, 'src/config_mgr.cpp'):
    h = read_file(cfg_h)
    cpp = read_file(cfg_cpp)
    check(contains(h, r'class\s+ConfigMgr'), 'class ConfigMgr declared', 'class ConfigMgr NOT found')
    for method in ['load', 'save', 'getWifiSSID', 'getWifiPass', 'getWifiCount',
                   'getToken', 'getUploadUrl', 'getDeviceId', 'getReadingIntervalMs']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')
    for method in ['load', 'save', 'getWifiSSID', 'getWifiPass', 'getWifiCount',
                   'getToken', 'getUploadUrl', 'getDeviceId', 'getReadingIntervalMs']:
        check(contains(cpp, rf'ConfigMgr::{method}\b'), f'{method}() implemented in cpp',
              f'{method}() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 3: buffer.h/cpp — ReadingBuffer class
# ---------------------------------------------------------------------------
print("\n=== Test 3: src/buffer.h/cpp — ReadingBuffer ===")
buf_h   = os.path.join(SRC_DIR, 'buffer.h')
buf_cpp = os.path.join(SRC_DIR, 'buffer.cpp')
if check_file(buf_h, 'src/buffer.h') and check_file(buf_cpp, 'src/buffer.cpp'):
    h = read_file(buf_h)
    cpp = read_file(buf_cpp)
    check(contains(h, r'class\s+ReadingBuffer'), 'class ReadingBuffer declared', 'class ReadingBuffer NOT found')
    for method in ['begin', 'append', 'count', 'unsentCount', 'markSent', 'getUnsent']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')
    for method in ['begin', 'append', 'count', 'unsentCount', 'markSent', 'getUnsent']:
        check(contains(cpp, rf'ReadingBuffer::{method}\b'), f'{method}() implemented in cpp',
              f'{method}() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 4: wifi_mgr.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 4: src/wifi_mgr.h/cpp ===")
wifi_h   = os.path.join(SRC_DIR, 'wifi_mgr.h')
wifi_cpp = os.path.join(SRC_DIR, 'wifi_mgr.cpp')
if check_file(wifi_h, 'src/wifi_mgr.h') and check_file(wifi_cpp, 'src/wifi_mgr.cpp'):
    h = read_file(wifi_h)
    cpp = read_file(wifi_cpp)
    check(contains(h, r'class\s+WifiMgr'), 'class WifiMgr declared', 'class WifiMgr NOT found')
    for method in ['begin', 'isStaConnected', 'getStaRSSI', 'getApClients']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')
    for method in ['begin', 'isStaConnected', 'getStaRSSI', 'getApClients']:
        check(contains(cpp, rf'WifiMgr::{method}\b'), f'{method}() implemented in cpp',
              f'{method}() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 5: led.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 5: src/led.h/cpp ===")
led_h   = os.path.join(SRC_DIR, 'led.h')
led_cpp = os.path.join(SRC_DIR, 'led.cpp')
if check_file(led_h, 'src/led.h') and check_file(led_cpp, 'src/led.cpp'):
    h = read_file(led_h)
    check(contains(h, r'class\s+Led'), 'class Led declared', 'class Led NOT found')
    check(contains(h, r'LedPattern'), 'LedPattern enum declared', 'LedPattern enum NOT found')
    for pattern in ['SOLID', 'SLOW_BLINK', 'FAST_BLINK', 'DOUBLE_FLASH', 'OFF']:
        check(contains(h, rf'\b{pattern}\b'), f'pattern {pattern} defined', f'pattern {pattern} MISSING')
    for method in ['begin', 'setPattern', 'update']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')


# ---------------------------------------------------------------------------
# Test 6: uploader.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 6: src/uploader.h/cpp ===")
up_h   = os.path.join(SRC_DIR, 'uploader.h')
up_cpp = os.path.join(SRC_DIR, 'uploader.cpp')
if check_file(up_h, 'src/uploader.h') and check_file(up_cpp, 'src/uploader.cpp'):
    h = read_file(up_h)
    cpp = read_file(up_cpp)
    check(contains(h, r'class\s+Uploader'), 'class Uploader declared', 'class Uploader NOT found')
    for method in ['begin', 'isUploading']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')
    for method in ['begin', 'isUploading']:
        check(contains(cpp, rf'Uploader::{method}\b'), f'{method}() implemented in cpp',
              f'{method}() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 7: radiacode.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 7: src/radiacode.h/cpp ===")
rc_h   = os.path.join(SRC_DIR, 'radiacode.h')
rc_cpp = os.path.join(SRC_DIR, 'radiacode.cpp')
if check_file(rc_h, 'src/radiacode.h') and check_file(rc_cpp, 'src/radiacode.cpp'):
    h = read_file(rc_h)
    check(contains(h, r'class\s+RadiaCode'), 'class RadiaCode declared', 'class RadiaCode NOT found')
    for method in ['connect', 'disconnect', 'isConnected', 'execute', 'buildRequest']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')


# ---------------------------------------------------------------------------
# Test 8: gps/gps.h and gps/atgm336h.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 8: src/gps/gps.h and src/gps/atgm336h.h/cpp ===")
gps_h     = os.path.join(SRC_DIR, 'gps', 'gps.h')
atgm_h    = os.path.join(SRC_DIR, 'gps', 'atgm336h.h')
atgm_cpp  = os.path.join(SRC_DIR, 'gps', 'atgm336h.cpp')
if check_file(gps_h, 'src/gps/gps.h'):
    h = read_file(gps_h)
    check(contains(h, r'class\s+GPS'), 'abstract class GPS declared', 'class GPS NOT found')
    for method in ['begin', 'poll', 'hasFix', 'getLat', 'getLon', 'getAlt',
                   'getSpeed', 'getHeading', 'getAccuracy', 'getSatellites']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in GPS interface',
              f'{method}() NOT in GPS interface')
    check(contains(h, r'virtual'), 'GPS uses virtual methods', 'GPS missing virtual methods')

if check_file(atgm_h, 'src/gps/atgm336h.h') and check_file(atgm_cpp, 'src/gps/atgm336h.cpp'):
    h = read_file(atgm_h)
    cpp = read_file(atgm_cpp)
    check(contains(h, r'class\s+ATGM336H'), 'class ATGM336H declared', 'class ATGM336H NOT found')
    check(contains(h, r':\s*public\s+GPS'), 'ATGM336H inherits GPS', 'ATGM336H does not inherit GPS')
    for method in ['begin', 'poll', 'hasFix', 'getLat', 'getLon']:
        check(contains(cpp, rf'ATGM336H::{method}\b'), f'{method}() implemented in cpp',
              f'{method}() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 9: portal/portal.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 9: src/portal/portal.h/cpp ===")
portal_h   = os.path.join(SRC_DIR, 'portal', 'portal.h')
portal_cpp = os.path.join(SRC_DIR, 'portal', 'portal.cpp')
if check_file(portal_h, 'src/portal/portal.h') and check_file(portal_cpp, 'src/portal/portal.cpp'):
    h = read_file(portal_h)
    cpp = read_file(portal_cpp)
    check(contains(h, r'class\s+StatusPortal'), 'class StatusPortal declared', 'class StatusPortal NOT found')
    check(contains(h, r'\bbegin\s*\('), 'begin() in header', 'begin() NOT in header')
    check(contains(cpp, r'StatusPortal::begin\b'), 'begin() implemented in cpp',
          'begin() NOT implemented in cpp')


# ---------------------------------------------------------------------------
# Test 10: portal/debug_ws.h/cpp
# ---------------------------------------------------------------------------
print("\n=== Test 10: src/portal/debug_ws.h/cpp ===")
dws_h   = os.path.join(SRC_DIR, 'portal', 'debug_ws.h')
dws_cpp = os.path.join(SRC_DIR, 'portal', 'debug_ws.cpp')
if check_file(dws_h, 'src/portal/debug_ws.h') and check_file(dws_cpp, 'src/portal/debug_ws.cpp'):
    h = read_file(dws_h)
    cpp = read_file(dws_cpp)
    check(contains(h, r'class\s+DebugWS'), 'class DebugWS declared', 'class DebugWS NOT found')
    for method in ['begin', 'log', 'setLevel']:
        check(contains(h, rf'\b{method}\s*\('), f'{method}() in header', f'{method}() NOT in header')
    check(contains(h, r'extern\s+DebugWS\s+debugWS'), 'global debugWS instance declared',
          'global debugWS NOT declared')


# ---------------------------------------------------------------------------
# Test 11: main.cpp includes all modules
# ---------------------------------------------------------------------------
print("\n=== Test 11: main.cpp integrates all modules ===")
main_cpp = os.path.join(SRC_DIR, 'main.cpp')
if check_file(main_cpp, 'src/main.cpp'):
    m = read_file(main_cpp)
    for inc in ['reading.h', 'config_mgr.h', 'buffer.h', 'wifi_mgr.h',
                'led.h', 'uploader.h', 'radiacode.h',
                'gps/atgm336h.h', 'portal/portal.h', 'portal/debug_ws.h']:
        check(contains(m, rf'#include\s+["\<]{re.escape(inc)}["\>]'),
              f'main.cpp includes {inc}',
              f'main.cpp MISSING include: {inc}')
    check(contains(m, r'\bsetup\s*\(\s*\)'), 'main.cpp has setup()', 'main.cpp MISSING setup()')
    check(contains(m, r'\bloop\s*\(\s*\)'), 'main.cpp has loop()', 'main.cpp MISSING loop()')


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print(f"\n=== Results: {PASS} passed, {FAIL} failed ===")
if FAIL > 0:
    sys.exit(1)
else:
    print("All module stub tests passed! ✅")
    sys.exit(0)

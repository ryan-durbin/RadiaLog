#!/usr/bin/env python3
"""
Test: debug_ws structure verification
Verifies that portal/debug_ws.h and portal/debug_ws.cpp contain the expected
declarations and implementation details.
"""

import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HEADER_PATH = os.path.join(REPO_ROOT, 'firmware', 'src', 'portal', 'debug_ws.h')
SOURCE_PATH = os.path.join(REPO_ROOT, 'firmware', 'src', 'portal', 'debug_ws.cpp')

errors = []


def check_file_exists(path, label):
    if not os.path.isfile(path):
        errors.append(f"MISSING: {label} ({path})")
        return False
    return True


def check_contains(path, needle, label):
    with open(path, 'r') as f:
        content = f.read()
    if needle not in content:
        errors.append(f"NOT FOUND in {os.path.basename(path)}: {label!r}")
        return False
    return True


# ── File existence ────────────────────────────────────────────────────────────
header_ok = check_file_exists(HEADER_PATH, 'portal/debug_ws.h')
source_ok  = check_file_exists(SOURCE_PATH, 'portal/debug_ws.cpp')

# ── Header checks ─────────────────────────────────────────────────────────────
if header_ok:
    check_contains(HEADER_PATH, 'class DebugWS',         'class DebugWS declaration')
    check_contains(HEADER_PATH, 'enum LogLevel',          'LogLevel enum')
    check_contains(HEADER_PATH, 'enum LogModule',         'LogModule enum')
    check_contains(HEADER_PATH, 'ERROR',                  'LogLevel::ERROR value')
    check_contains(HEADER_PATH, 'WARN',                   'LogLevel::WARN value')
    check_contains(HEADER_PATH, 'INFO',                   'LogLevel::INFO value')
    check_contains(HEADER_PATH, 'DEBUG',                  'LogLevel::DEBUG value')
    check_contains(HEADER_PATH, 'USB',                    'LogModule::USB value')
    check_contains(HEADER_PATH, 'GPS',                    'LogModule::GPS value')
    check_contains(HEADER_PATH, 'WIFI',                   'LogModule::WIFI value')
    check_contains(HEADER_PATH, 'UPLOAD',                 'LogModule::UPLOAD value')
    check_contains(HEADER_PATH, 'BUFFER',                 'LogModule::BUFFER value')
    check_contains(HEADER_PATH, 'begin(',                 'begin() method declaration')
    check_contains(HEADER_PATH, 'log(',                   'log() method declaration')
    check_contains(HEADER_PATH, 'setLevel(',              'setLevel() method declaration')
    check_contains(HEADER_PATH, 'extern DebugWS debugWS', 'global extern DebugWS debugWS')
    check_contains(HEADER_PATH, 'DEBUG_WS_BUFFER_SIZE',   'buffer size constant')
    check_contains(HEADER_PATH, '500',                    'buffer size value 500')

# ── Source checks ─────────────────────────────────────────────────────────────
if source_ok:
    check_contains(SOURCE_PATH, '/ws/debug',              'WebSocket path /ws/debug')
    check_contains(SOURCE_PATH, 'DEBUG_WS_BUFFER_SIZE',   'circular buffer using DEBUG_WS_BUFFER_SIZE')
    check_contains(SOURCE_PATH, '_buffer[',               'circular buffer array _buffer')
    check_contains(SOURCE_PATH, '_head',                  'circular buffer head pointer _head')
    check_contains(SOURCE_PATH, '_count',                 'circular buffer count _count')
    check_contains(SOURCE_PATH, 'WS_EVT_CONNECT',         'onEvent WS_EVT_CONNECT handler')
    check_contains(SOURCE_PATH, 'Serial.println',         'Serial output in log()')
    check_contains(SOURCE_PATH, 'textAll',                'WebSocket broadcast textAll()')
    check_contains(SOURCE_PATH, '_minLevel',              'min level filtering _minLevel')
    check_contains(SOURCE_PATH, '"ts"',                   'JSON ts field')
    check_contains(SOURCE_PATH, '"module"',               'JSON module field')
    check_contains(SOURCE_PATH, '"level"',                'JSON level field')
    check_contains(SOURCE_PATH, '"msg"',                  'JSON msg field')

# ── Report ────────────────────────────────────────────────────────────────────
if errors:
    print("FAIL - debug_ws structure checks:")
    for e in errors:
        print(f"  ✗ {e}")
    sys.exit(1)
else:
    print("PASS - all debug_ws structure checks passed")
    sys.exit(0)

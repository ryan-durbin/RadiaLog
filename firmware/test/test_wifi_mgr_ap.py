#!/usr/bin/env python3
"""
Tests for WiFi Manager AP mode implementation (US-002).
Validates wifi_mgr.h and wifi_mgr.cpp structure and content.
"""

import os
import sys
import re

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC_DIR = os.path.join(REPO_ROOT, 'firmware', 'src')

WIFI_MGR_H = os.path.join(SRC_DIR, 'wifi_mgr.h')
WIFI_MGR_CPP = os.path.join(SRC_DIR, 'wifi_mgr.cpp')

tests_run = 0
tests_passed = 0
tests_failed = 0


def test(name, condition, detail=""):
    global tests_run, tests_passed, tests_failed
    tests_run += 1
    if condition:
        tests_passed += 1
        print(f"  PASS  {name}")
    else:
        tests_failed += 1
        print(f"  FAIL  {name}" + (f": {detail}" if detail else ""))


def read_file(path):
    with open(path, 'r') as f:
        return f.read()


print("\n=== WiFi Manager AP Mode Tests (US-002) ===\n")

# --- File existence ---
print("[1] File existence")
test("wifi_mgr.h exists", os.path.isfile(WIFI_MGR_H))
test("wifi_mgr.cpp exists", os.path.isfile(WIFI_MGR_CPP))

if not os.path.isfile(WIFI_MGR_H) or not os.path.isfile(WIFI_MGR_CPP):
    print("\nCRITICAL: Required files missing. Aborting remaining tests.")
    sys.exit(1)

header = read_file(WIFI_MGR_H)
source = read_file(WIFI_MGR_CPP)

# --- Header checks ---
print("\n[2] Header guards and includes")
test("Header has #pragma once", "#pragma once" in header)
test("Header includes WiFi.h", '#include <WiFi.h>' in header)
test("Header includes config.h", '#include "config.h"' in header)

# --- Class declaration ---
print("\n[3] WifiMgr class declaration")
test("WifiMgr class declared", "class WifiMgr" in header)
test("begin() method declared", re.search(r'void\s+begin\s*\(', header) is not None)
test("getAPIP() method declared", re.search(r'getAPIP\s*\(', header) is not None)
test("isSTAConnected() method declared", re.search(r'isSTAConnected\s*\(', header) is not None)
test("getSTAIP() method declared", re.search(r'getSTAIP\s*\(', header) is not None)
test("getSignalStrength() method declared", re.search(r'getSignalStrength\s*\(', header) is not None)
test("getSSID() method declared", re.search(r'getSSID\s*\(', header) is not None)

# --- AP password support ---
print("\n[4] Optional AP password support")
test("begin() accepts optional password parameter",
     re.search(r'begin\s*\(.*[Pp]ass', header) is not None or
     re.search(r'begin\s*\(\s*const\s+String', header) is not None,
     "begin() should accept optional apPassword param")

# --- Source checks: WiFi mode ---
print("\n[5] WiFi mode configuration (source)")
test("WIFI_AP_STA mode used", "WIFI_AP_STA" in source)
test("WiFi.mode() called", re.search(r'WiFi\.mode\s*\(', source) is not None)

# --- AP SSID construction ---
print("\n[6] AP SSID construction")
test("AP_SSID_PREFIX from config.h is referenced", "AP_SSID_PREFIX" in source)
test("MAC address used for suffix", re.search(r'macAddress|getMac|mac\[', source, re.IGNORECASE) is not None)
test("SSID built with prefix + suffix",
     re.search(r'AP_SSID_PREFIX.*\+|String\(AP_SSID_PREFIX\)', source) is not None)

# --- softAP and softAPConfig ---
print("\n[7] AP setup calls")
test("softAP() called", re.search(r'WiFi\.softAP\s*\(', source) is not None)
test("softAPConfig() called", re.search(r'WiFi\.softAPConfig\s*\(', source) is not None)

# --- AP channel ---
print("\n[8] AP channel configuration")
test("AP_CHANNEL used", "AP_CHANNEL" in source)

# --- AP IP address ---
print("\n[9] AP IP address")
test("10.0.0.1 configured as AP IP",
     "10, 0, 0, 1" in source or "10.0.0.1" in source)

# --- getAPIP implementation ---
print("\n[10] getAPIP implementation")
test("getAPIP() returns softAPIP",
     re.search(r'getAPIP.*\{.*softAPIP|softAPIP.*\(\)', source, re.DOTALL) is not None or
     "softAPIP()" in source)

# --- Open network support ---
print("\n[11] Open network support")
test("Empty password results in open network",
     re.search(r'nullptr|password\.length.*==.*0|password\s*==\s*""|\"\"\)', source) is not None)

# --- Summary ---
print(f"\n{'='*45}")
print(f"Results: {tests_passed}/{tests_run} passed, {tests_failed} failed")
print(f"{'='*45}\n")

if tests_failed > 0:
    sys.exit(1)
else:
    print("All WiFi Manager AP mode tests passed!")
    sys.exit(0)

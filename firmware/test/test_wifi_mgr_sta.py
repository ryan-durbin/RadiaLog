#!/usr/bin/env python3
"""
Tests for WiFi Manager STA mode (US-003).
Validates multi-network support, event handling, and callback registration.
"""

import os
import re
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HEADER = os.path.join(REPO_ROOT, "firmware", "src", "wifi_mgr.h")
SOURCE = os.path.join(REPO_ROOT, "firmware", "src", "wifi_mgr.cpp")

# -----------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------

def read_file(path):
    with open(path, "r") as f:
        return f.read()

pass_count = 0
fail_count = 0

def test(name, condition, detail=""):
    global pass_count, fail_count
    if condition:
        print(f"  PASS  {name}")
        pass_count += 1
    else:
        print(f"  FAIL  {name}" + (f": {detail}" if detail else ""))
        fail_count += 1

# -----------------------------------------------------------------------
# Load files
# -----------------------------------------------------------------------

print("=== WiFi Manager STA Mode Tests ===\n")

try:
    header = read_file(HEADER)
    source = read_file(SOURCE)
except FileNotFoundError as e:
    print(f"ERROR: Could not open file: {e}")
    sys.exit(1)

# -----------------------------------------------------------------------
# 1. File basics
# -----------------------------------------------------------------------
print("[1] File structure")
test("wifi_mgr.h exists", os.path.isfile(HEADER))
test("wifi_mgr.cpp exists", os.path.isfile(SOURCE))
test("wifi_mgr.h uses #pragma once", "#pragma once" in header)
test("wifi_mgr.cpp includes wifi_mgr.h", '#include "wifi_mgr.h"' in source)

# -----------------------------------------------------------------------
# 2. WifiCredentials struct
# -----------------------------------------------------------------------
print("\n[2] WifiCredentials struct")
test("WifiCredentials struct declared", "struct WifiCredentials" in header)
test("WifiCredentials has ssid field", re.search(r'String\s+ssid', header) is not None)
test("WifiCredentials has password field", re.search(r'String\s+password', header) is not None)
test("WifiCredentials has priority field", re.search(r'uint8_t\s+priority', header) is not None)

# -----------------------------------------------------------------------
# 3. Multi-network support
# -----------------------------------------------------------------------
print("\n[3] Multi-network support")
test("MAX_NETWORKS = 3 defined", "MAX_NETWORKS = 3" in header)
test("setNetworks() method declared", "setNetworks(" in header)
test("setNetworks accepts vector", re.search(r'setNetworks\s*\(\s*const\s+std::vector', header) is not None)
test("networks array in header", re.search(r'_networks\s*\[', header) is not None)
test("networkCount tracked", "_networkCount" in header)
test("setNetworks implemented in source", "setNetworks(" in source)
test("networks sorted by priority", "sort(" in source or "std::sort(" in source)

# -----------------------------------------------------------------------
# 4. STA connection methods
# -----------------------------------------------------------------------
print("\n[4] STA connection methods")
test("isSTAConnected() declared", "isSTAConnected()" in header)
test("isSTAConnected() implemented", "isSTAConnected()" in source)
test("getSTAIP() declared", "getSTAIP()" in header)
test("getSTAIP() implemented", "getSTAIP()" in source)
test("getSignalStrength() declared", "getSignalStrength()" in header)
test("getSignalStrength() implemented", "getSignalStrength()" in source)
test("getSSID() declared", "getSSID()" in header)
test("getSSID() implemented", "getSSID()" in source)
test("connectSTA() method exists", "connectSTA()" in header and "connectSTA()" in source)

# -----------------------------------------------------------------------
# 5. WiFi event handling
# -----------------------------------------------------------------------
print("\n[5] WiFi event handling")
test("WiFi.onEvent() used for event registration", "WiFi.onEvent(" in source)
test("Static event handler method declared", "_wifiEventHandler" in header)
test("Static event handler implemented", "_wifiEventHandler" in source)
test("Handles STA_GOT_IP event", "ARDUINO_EVENT_WIFI_STA_GOT_IP" in source or "SYSTEM_EVENT_STA_GOT_IP" in source)
test("Handles STA_DISCONNECTED event", "ARDUINO_EVENT_WIFI_STA_DISCONNECTED" in source or "SYSTEM_EVENT_STA_DISCONNECTED" in source)
test("Static instance pointer for handler", "_instance" in header and "_instance" in source)

# -----------------------------------------------------------------------
# 6. Connect/disconnect callbacks
# -----------------------------------------------------------------------
print("\n[6] Connect/disconnect callbacks")
test("WifiEventCallback typedef declared", "WifiEventCallback" in header)
test("registerOnConnect() declared", "registerOnConnect(" in header)
test("registerOnConnect() implemented", "registerOnConnect(" in source)
test("registerOnDisconnect() declared", "registerOnDisconnect(" in header)
test("registerOnDisconnect() implemented", "registerOnDisconnect(" in source)
test("_onConnectCb member exists", "_onConnectCb" in header)
test("_onDisconnectCb member exists", "_onDisconnectCb" in header)
test("onConnectCb invoked in source", "_onConnectCb()" in source)
test("onDisconnectCb invoked in source", "_onDisconnectCb()" in source)

# -----------------------------------------------------------------------
# 7. Auto-reconnect logic
# -----------------------------------------------------------------------
print("\n[7] Auto-reconnect logic")
test("update() method declared", "update()" in header)
test("update() implemented", "update()" in source)
test("_reconnectPending flag exists", "_reconnectPending" in header)
test("_reconnectAt timing field exists", "_reconnectAt" in header)
test("_connectingNow state tracked", "_connectingNow" in header)
test("_tryNextNetwork() helper exists", "_tryNextNetwork(" in header and "_tryNextNetwork(" in source)
test("Reconnect uses millis()", "millis()" in source)

# -----------------------------------------------------------------------
# 8. Priority fallback
# -----------------------------------------------------------------------
print("\n[8] Priority fallback")
test("_currentNetworkIndex tracks position", "_currentNetworkIndex" in header)
test("Falls back to next network on timeout", "_currentNetworkIndex++" in source or "_currentNetworkIndex +=" in source)
test("Restarts from index 0 on disconnect", "_currentNetworkIndex = 0" in source)

# -----------------------------------------------------------------------
# 9. Exponential backoff
# -----------------------------------------------------------------------
print("\n[9] Exponential backoff")
test("_reconnectDelay member exists", "_reconnectDelay" in header)
test("Backoff min delay defined", "RECONNECT_DELAY_MIN_MS" in header)
test("Backoff max delay defined", "RECONNECT_DELAY_MAX_MS" in header)
test("Delay doubles on failure", "_reconnectDelay *= 2" in source)
test("Backoff capped at max", "RECONNECT_DELAY_MAX_MS" in source)
test("Backoff resets on successful connect", "RECONNECT_DELAY_MIN_MS" in source)

# -----------------------------------------------------------------------
# 10. No blocking delay()
# -----------------------------------------------------------------------
print("\n[10] Non-blocking implementation")
# Check that delay() is not used in reconnect logic
# We allow delay() in includes/macros but not as a function call in .cpp
delay_calls = re.findall(r'\bdelay\s*\(', source)
test("No blocking delay() calls in wifi_mgr.cpp", len(delay_calls) == 0,
     f"Found {len(delay_calls)} delay() call(s)")

# -----------------------------------------------------------------------
# 11. AP mode preserved
# -----------------------------------------------------------------------
print("\n[11] AP mode preserved from US-002")
test("begin() method still present", "begin(" in header)
test("_setupAP() still present", "_setupAP(" in header and "_setupAP(" in source)
test("getAPIP() still present", "getAPIP()" in header and "getAPIP()" in source)
test("softAP called in source", "softAP(" in source)
test("10.0.0.1 portal IP configured", "10, 0, 0, 1" in source)
test("AP_CHANNEL used", "AP_CHANNEL" in source)

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
total = pass_count + fail_count
print(f"\n=== Results: {pass_count}/{total} tests passed ===")

if fail_count > 0:
    sys.exit(1)
else:
    print("All tests passed!")
    sys.exit(0)

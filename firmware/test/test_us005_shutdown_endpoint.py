#!/usr/bin/env python3
"""
US-005: Tests for POST /api/actions/shutdown endpoint in portal.cpp
Verifies source-level correctness: endpoint registration, response format,
no direct deep-sleep call, and flag mechanism wired to main loop.
"""

import re
import sys

PORTAL_CPP = "src/portal/portal.cpp"
PORTAL_H   = "src/portal/portal.h"
MAIN_CPP   = "src/main.cpp"

def read(path):
    with open(path) as f:
        return f.read()

portal_cpp = read(PORTAL_CPP)
portal_h   = read(PORTAL_H)
main_cpp   = read(MAIN_CPP)

failures = []

def check(condition, description):
    if condition:
        print(f"✅ {description}")
    else:
        print(f"❌ {description}")
        failures.append(description)

# AC1: POST /api/actions/shutdown endpoint exists in portal.cpp
check(
    '/api/actions/shutdown' in portal_cpp and 'HTTP_POST' in portal_cpp,
    "AC1: POST /api/actions/shutdown endpoint registered in portal.cpp"
)

# More specific: the shutdown route uses HTTP_POST
shutdown_route_match = re.search(
    r'on\s*\(\s*"/api/actions/shutdown"\s*,\s*HTTP_POST',
    portal_cpp
)
check(bool(shutdown_route_match), "AC1b: Route uses HTTP_POST method")

# AC2: Endpoint returns JSON response with success:true
# In C++ source, JSON is embedded as a string literal with escaped quotes:
# e.g. "{\"success\":true,\"message\":\"Entering shipping mode...\"}"
check(
    ('success":true' in portal_cpp or '\\"success\\":true' in portal_cpp or '"success":true' in portal_cpp)
    and 'Entering shipping mode' in portal_cpp,
    "AC2: Response includes success:true and shipping mode message"
)

# AC3: Endpoint does NOT directly call esp_deep_sleep_start()
# Find the shutdown handler body and confirm no deep_sleep call inside it
# Strategy: find handler block between /api/actions/shutdown and next _server->on
shutdown_block_match = re.search(
    r'on\s*\(\s*"/api/actions/shutdown".*?\}\s*\)\s*;',
    portal_cpp, re.DOTALL
)
if shutdown_block_match:
    shutdown_block = shutdown_block_match.group(0)
    check(
        'esp_deep_sleep_start' not in shutdown_block,
        "AC3: Shutdown handler does NOT call esp_deep_sleep_start() directly"
    )
else:
    check(False, "AC3: Could not locate shutdown handler block to verify")

# AC4: A flag/signal mechanism exists (volatile bool)
check(
    'g_shutdownRequested' in portal_cpp,
    "AC4a: g_shutdownRequested flag defined in portal.cpp"
)
check(
    'volatile bool g_shutdownRequested' in portal_cpp,
    "AC4b: Flag declared as volatile bool"
)
check(
    'extern volatile bool g_shutdownRequested' in portal_h,
    "AC4c: Flag declared extern in portal.h for main loop access"
)

# AC4d: Shutdown handler sets the flag
check(
    'g_shutdownRequested = true' in portal_cpp,
    "AC4d: Handler sets g_shutdownRequested = true"
)

# AC5: Main loop checks the shutdown flag and triggers the same LED + sleep sequence
check(
    'g_shutdownRequested' in main_cpp,
    "AC5a: main.cpp references g_shutdownRequested"
)
check(
    'portal/portal.h' in main_cpp,
    "AC5b: main.cpp includes portal.h (which exposes the extern flag)"
)

# The flag check is in the same if-condition as shippingMode.shouldEnterSleep()
combined_check = re.search(
    r'shouldEnterSleep\(\)\s*\|\|\s*g_shutdownRequested|g_shutdownRequested\s*\|\|\s*shouldEnterSleep\(\)',
    main_cpp
)
check(bool(combined_check), "AC5c: Main loop checks g_shutdownRequested alongside shouldEnterSleep()")

# Deep sleep is called in the same block
sleep_block_match = re.search(
    r'(shouldEnterSleep\(\)\s*\|\|\s*g_shutdownRequested|g_shutdownRequested\s*\|\|\s*shouldEnterSleep\(\))'
    r'.*?esp_deep_sleep_start\(\)',
    main_cpp, re.DOTALL
)
check(bool(sleep_block_match), "AC5d: esp_deep_sleep_start() called when flag is set")

# Flag is cleared in the main loop block to prevent stuck state
check(
    'g_shutdownRequested = false' in main_cpp,
    "AC5e: Main loop clears g_shutdownRequested after triggering sleep"
)

# AC6: Endpoint follows same pattern as /api/actions/reboot (response before action)
reboot_response = re.search(
    r'on\s*\(\s*"/api/actions/reboot".*?request->send\(',
    portal_cpp, re.DOTALL
)
shutdown_response = re.search(
    r'on\s*\(\s*"/api/actions/shutdown".*?request->send\(',
    portal_cpp, re.DOTALL
)
check(bool(reboot_response) and bool(shutdown_response),
    "AC6: Both /reboot and /shutdown handlers call request->send()")

print()
if failures:
    print(f"FAILED: {len(failures)} check(s) failed:")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)
else:
    print("All US-005 shutdown endpoint tests passed!")

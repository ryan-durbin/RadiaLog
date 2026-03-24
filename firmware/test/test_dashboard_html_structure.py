#!/usr/bin/env python3
"""Test that dashboard_html.h has correct structure and required elements."""

import sys
import os

HEADER_PATH = os.path.join(
    os.path.dirname(__file__),
    "..", "src", "portal", "html", "dashboard_html.h"
)

def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)

def check(content, needle, label):
    if needle not in content:
        fail(f"Missing {label!r} (expected to find: {needle!r})")

# 1. File exists
if not os.path.isfile(HEADER_PATH):
    fail(f"File not found: {HEADER_PATH}")

with open(HEADER_PATH, "r", encoding="utf-8") as f:
    content = f.read()

# 2. PROGMEM keyword and array name
check(content, "PROGMEM", "PROGMEM keyword")
check(content, "DASHBOARD_HTML", "DASHBOARD_HTML array")
check(content, "DASHBOARD_HTML[]", "DASHBOARD_HTML[] array declaration")

# 3. fetch('/api/status')
check(content, "fetch('/api/status')", "fetch('/api/status') call")

# 4. setInterval for auto-refresh
check(content, "setInterval", "setInterval for auto-refresh")

# 5. Required element ids
required_ids = [
    "dose-rate",
    "count-rate",
    "gps-fix",
    "gps-lat",
    "gps-lon",
    "buffer-pending",
    "last-upload",
]
for eid in required_ids:
    check(content, f'id="{eid}"', f'element id="{eid}"')

# 6. Nav links to all pages
nav_links = [
    'href="/"',
    'href="/debug"',
    'href="/settings"',
    'href="/data"',
]
for link in nav_links:
    check(content, link, f"nav link {link}")

# 7. 2000ms refresh interval
check(content, "2000", "2000ms refresh interval")

# 8. Connection lost indicator
check(content, "conn-lost", "connection lost indicator element")

# 9. GPS sats element
check(content, 'id="gps-sats"', 'element id="gps-sats"')

# 10. WiFi status elements
check(content, 'id="wifi-status"', 'element id="wifi-status"')
check(content, 'id="wifi-rssi"', 'element id="wifi-rssi"')

# 11. RadiaMaps elements
check(content, 'id="rm-username"', 'element id="rm-username"')
check(content, 'id="rm-subscription"', 'element id="rm-subscription"')
check(content, 'id="rm-lifetime"', 'element id="rm-lifetime"')

# 12. buffer-total element
check(content, 'id="buffer-total"', 'element id="buffer-total"')

print("OK: All dashboard_html.h structure checks passed.")
sys.exit(0)

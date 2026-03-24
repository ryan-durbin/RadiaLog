#!/usr/bin/env python3
"""Test that firmware/src/portal/html/shared_css.h has the required structure."""

import sys
import os

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HEADER = os.path.join(REPO_ROOT, 'firmware', 'src', 'portal', 'html', 'shared_css.h')

def fail(msg):
    print(f"FAIL: {msg}")
    sys.exit(1)

def ok(msg):
    print(f"OK: {msg}")

# 1. File exists
if not os.path.isfile(HEADER):
    fail(f"shared_css.h not found at {HEADER}")
ok("shared_css.h exists")

with open(HEADER, 'r', encoding='utf-8') as f:
    content = f.read()

# 2. PROGMEM keyword present
if 'PROGMEM' not in content:
    fail("PROGMEM keyword not found")
ok("PROGMEM keyword present")

# 3. SHARED_CSS array name
if 'SHARED_CSS' not in content:
    fail("SHARED_CSS array name not found")
ok("SHARED_CSS array declared")

# 4. NAV_TEMPLATE array name
if 'NAV_TEMPLATE' not in content:
    fail("NAV_TEMPLATE array name not found")
ok("NAV_TEMPLATE array declared")

# 5. Log level color classes
if '.level-error' not in content:
    fail(".level-error class not found")
ok(".level-error class present")

if '.level-warn' not in content:
    fail(".level-warn class not found")
ok(".level-warn class present")

if '.level-info' not in content:
    fail(".level-info class not found")
ok(".level-info class present")

if '.level-debug' not in content:
    fail(".level-debug class not found")
ok(".level-debug class present")

# 6. Nav links to all four pages
for path in ['href="/"', 'href="/debug"', 'href="/settings"', 'href="/data"']:
    if path not in content:
        fail(f"Nav link {path} not found in NAV_TEMPLATE")
    ok(f"Nav link {path} present")

# 7. Mobile responsive (max-width or media query)
if 'max-width' not in content and '@media' not in content:
    fail("No mobile-responsive CSS (max-width or @media) found")
ok("Mobile-responsive CSS present")

# 8. Status dot colors (green/red/gray)
for cls in ['.dot-green', '.dot-red', '.dot-gray']:
    if cls not in content:
        fail(f"Status dot color class {cls} not found")
    ok(f"Status dot class {cls} present")

# 9. Utility classes
for cls in ['.btn', '.btn-danger', '.btn-primary', '.status-ok', '.status-err']:
    if cls not in content:
        fail(f"Utility class {cls} not found")
    ok(f"Utility class {cls} present")

# 10. %DEVICE_NAME% and %STATUS_INDICATORS% placeholders in NAV_TEMPLATE
for placeholder in ['%DEVICE_NAME%', '%STATUS_INDICATORS%']:
    if placeholder not in content:
        fail(f"Placeholder {placeholder} not found in template")
    ok(f"Placeholder {placeholder} present")

print("\nAll checks passed.")
sys.exit(0)

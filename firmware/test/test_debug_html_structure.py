#!/usr/bin/env python3
"""Tests for debug_html.h — Debug console page PROGMEM structure."""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FILE = os.path.join(REPO, "firmware", "src", "portal", "html", "debug_html.h")

def main():
    errors = []

    # 1. File exists
    if not os.path.isfile(FILE):
        print(f"FAIL: {FILE} does not exist")
        sys.exit(1)

    with open(FILE, "r") as f:
        content = f.read()

    def check(desc, condition):
        if not condition:
            errors.append(desc)
            print(f"  FAIL: {desc}")
        else:
            print(f"  OK: {desc}")

    # 2. PROGMEM and DEBUG_HTML array
    check("Contains PROGMEM keyword", "PROGMEM" in content)
    check("Contains DEBUG_HTML[] declaration", "DEBUG_HTML[]" in content or "DEBUG_HTML [" in content)

    # 3. WebSocket connection
    check("Contains new WebSocket constructor", "new WebSocket" in content)
    check("Contains /ws/debug endpoint", "/ws/debug" in content)

    # 4. Filter checkboxes for all 5 modules
    for mod in ["USB", "GPS", "WIFI", "UPLOAD", "BUFFER"]:
        check(f"Contains checkbox for {mod} module",
              f'filter-{mod}' in content and f'checkbox' in content)

    # 5. Level dropdown with all 4 levels
    check("Contains level-filter select element", "level-filter" in content)
    for lvl in ["DEBUG", "INFO", "WARN", "ERROR"]:
        check(f"Contains {lvl} option in dropdown",
              f'<option' in content and lvl in content)

    # 6. Log level CSS classes for color coding
    for lvl in ["ERROR", "WARN", "INFO", "DEBUG"]:
        check(f"Contains .level-{lvl} CSS class", f".level-{lvl}" in content)

    # 7. Auto-scroll logic
    check("Contains scrollHeight reference (auto-scroll)", "scrollHeight" in content)
    check("Contains autoScroll variable", "autoScroll" in content)

    # 8. Clear button
    check("Contains clear button", "btn-clear" in content or "Clear" in content)
    check("Clear empties log div (innerHTML)", "innerHTML" in content)

    # 9. Nav bar with links
    check("Contains nav element", "<nav" in content)
    check("Contains link to Dashboard /", 'href="/"' in content)
    check("Contains link to /debug", 'href="/debug"' in content)
    check("Contains link to /settings", 'href="/settings"' in content)
    check("Contains link to /data", 'href="/data"' in content)
    check("Debug link has active class", 'class="active"' in content and '/debug' in content)

    # 10. Log console div
    check("Contains log-console div", "log-console" in content)
    check("Uses monospace font", "monospace" in content)
    check("Log console has overflow-y:auto", "overflow-y:auto" in content)

    # 11. Reconnecting indicator
    check("Contains reconnecting indicator", "reconnect" in content.lower())

    # 12. Log entry format with data attributes
    check("Contains data-module attribute", "data-module" in content)
    check("Contains data-level attribute", "data-level" in content)
    check("Contains log-entry class", "log-entry" in content)

    # 13. Raw string literal for PROGMEM
    check("Uses raw string literal", 'R"rawhtml(' in content)

    if errors:
        print(f"\n{len(errors)} check(s) FAILED:")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)
    else:
        print(f"\nAll checks passed!")
        sys.exit(0)

if __name__ == "__main__":
    main()

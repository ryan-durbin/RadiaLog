"""
US-004: ShippingMode integration into main.cpp
Verifies that main.cpp correctly wires the ShippingMode module with LED
confirmation and deep sleep entry.
"""

import os
import sys
import re

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC_DIR = os.path.join(REPO_ROOT, 'firmware', 'src')

def read_file(path):
    with open(path, 'r') as f:
        return f.read()

def test_shipping_mode_header_included():
    """main.cpp must include shipping_mode.h"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    assert '#include "shipping_mode.h"' in main, "shipping_mode.h not included in main.cpp"
    print("✅ shipping_mode.h included")

def test_esp_sleep_header_included():
    """main.cpp must include esp_sleep.h"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    assert '#include <esp_sleep.h>' in main, "esp_sleep.h not included in main.cpp"
    print("✅ esp_sleep.h included")

def test_shipping_mode_instance_created():
    """A static ShippingMode instance must be declared in main.cpp"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    assert 'ShippingMode' in main and 'shippingMode' in main, \
        "ShippingMode instance not found in main.cpp"
    print("✅ ShippingMode instance declared")

def test_shipping_mode_begin_called():
    """shippingMode.begin() must be called in setup()"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    assert 'shippingMode.begin()' in main, "shippingMode.begin() not found in main.cpp"
    # Verify it's in setup() (appears before the closing of setup, before loop)
    setup_start = main.find('void setup()')
    loop_start = main.find('void loop()')
    begin_pos = main.find('shippingMode.begin()')
    assert setup_start < begin_pos < loop_start, \
        "shippingMode.begin() not in setup()"
    print("✅ shippingMode.begin() called in setup()")

def test_shipping_mode_begin_after_led_begin():
    """shippingMode.begin() must be called after led.begin()"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    led_begin_pos = main.find('led.begin()')
    ship_begin_pos = main.find('shippingMode.begin()')
    assert led_begin_pos != -1, "led.begin() not found"
    assert ship_begin_pos != -1, "shippingMode.begin() not found"
    assert ship_begin_pos > led_begin_pos, \
        "shippingMode.begin() must appear after led.begin()"
    print("✅ shippingMode.begin() called after led.begin()")

def test_shipping_mode_update_in_loop():
    """shippingMode.update() must be called in loop()"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    update_pos = main.find('shippingMode.update()', loop_start)
    assert update_pos != -1, "shippingMode.update() not found in loop()"
    print("✅ shippingMode.update() called in loop()")

def test_should_enter_sleep_checked():
    """shouldEnterSleep() must be checked in loop()"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    sleep_pos = main.find('shouldEnterSleep()', loop_start)
    assert sleep_pos != -1, "shouldEnterSleep() not found in loop()"
    print("✅ shouldEnterSleep() checked in loop()")

def test_triple_flash_set_on_sleep():
    """LedPattern::TRIPLE_FLASH must be set when entering shipping mode"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    triple_pos = main.find('TRIPLE_FLASH', loop_start)
    assert triple_pos != -1, "LedPattern::TRIPLE_FLASH not set in loop() shipping mode block"
    print("✅ LedPattern::TRIPLE_FLASH set in shipping mode block")

def test_is_pattern_complete_checked():
    """isPatternComplete() must be checked before entering sleep"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    complete_pos = main.find('isPatternComplete()', loop_start)
    assert complete_pos != -1, "isPatternComplete() not checked in loop()"
    print("✅ isPatternComplete() checked before sleep")

def test_flush_called_before_sleep():
    """configMgr.flushTotalReadingsLogged() must be called before deep sleep"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    flush_pos = main.find('flushTotalReadingsLogged()', loop_start)
    sleep_pos = main.find('esp_deep_sleep_start()', loop_start)
    assert flush_pos != -1, "flushTotalReadingsLogged() not found in loop()"
    assert sleep_pos != -1, "esp_deep_sleep_start() not found in loop()"
    assert flush_pos < sleep_pos, \
        "flushTotalReadingsLogged() must appear before esp_deep_sleep_start()"
    print("✅ flushTotalReadingsLogged() called before esp_deep_sleep_start()")

def test_deep_sleep_called():
    """esp_deep_sleep_start() must be called (no wakeup sources = reset only)"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    sleep_pos = main.find('esp_deep_sleep_start()', loop_start)
    assert sleep_pos != -1, "esp_deep_sleep_start() not found in loop()"
    # Verify no wakeup sources configured (no esp_sleep_enable_*_wakeup before it)
    sleep_block = main[loop_start:sleep_pos]
    wakeup_sources = re.findall(r'esp_sleep_enable_\w+_wakeup', sleep_block)
    assert len(wakeup_sources) == 0, \
        f"Unexpected wakeup source(s) configured: {wakeup_sources}"
    print("✅ esp_deep_sleep_start() called with no wakeup sources")

def test_debug_log_before_sleep():
    """Debug log message must be printed before entering deep sleep"""
    main = read_file(os.path.join(SRC_DIR, 'main.cpp'))
    loop_start = main.find('void loop()')
    log_pos = main.find('Entering shipping mode (deep sleep)', loop_start)
    sleep_pos = main.find('esp_deep_sleep_start()', loop_start)
    assert log_pos != -1, "Debug log 'Entering shipping mode (deep sleep)' not found"
    assert log_pos < sleep_pos, "Debug log must appear before esp_deep_sleep_start()"
    print("✅ Debug log printed before deep sleep")

def test_shipping_mode_files_exist():
    """shipping_mode.h and shipping_mode.cpp must exist"""
    assert os.path.exists(os.path.join(SRC_DIR, 'shipping_mode.h')), \
        "shipping_mode.h not found"
    assert os.path.exists(os.path.join(SRC_DIR, 'shipping_mode.cpp')), \
        "shipping_mode.cpp not found"
    print("✅ shipping_mode.h and shipping_mode.cpp exist")

if __name__ == '__main__':
    tests = [
        test_shipping_mode_header_included,
        test_esp_sleep_header_included,
        test_shipping_mode_instance_created,
        test_shipping_mode_begin_called,
        test_shipping_mode_begin_after_led_begin,
        test_shipping_mode_update_in_loop,
        test_should_enter_sleep_checked,
        test_triple_flash_set_on_sleep,
        test_is_pattern_complete_checked,
        test_flush_called_before_sleep,
        test_deep_sleep_called,
        test_debug_log_before_sleep,
        test_shipping_mode_files_exist,
    ]

    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as e:
            print(f"❌ {t.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"❌ {t.__name__}: unexpected error: {e}")
            failed += 1

    print(f"\n{'All tests passed!' if failed == 0 else f'{failed} test(s) FAILED'}")
    sys.exit(0 if failed == 0 else 1)

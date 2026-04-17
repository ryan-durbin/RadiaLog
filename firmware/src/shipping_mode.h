#pragma once

#include <Arduino.h>
#include "config.h"

// Short-press counter on BOOT_BUTTON_PIN. Incremented when the button is
// released before the shipping-mode hold threshold. Used by the self-test
// page in the captive portal to confirm the boot button works.
extern volatile uint32_t g_buttonPressCount;

// =============================================================================
// ShippingMode — GPIO0 long-press detection for deep-sleep entry
//
// Monitors BOOT_BUTTON_PIN (GPIO0, active LOW) and sets a flag when the button
// is held continuously for SHIPPING_MODE_HOLD_MS (5000ms).  The main loop
// calls update() each iteration and checks shouldEnterSleep() to decide
// whether to trigger the LED confirmation sequence and enter deep sleep.
//
// NOTE: On boards with HAS_DISPLAY where BUTTON2_PIN == 0 (T-Display S3,
//       T-Display S3 AMOLED), GPIO0 is shared between this module and the
//       normal button handler.  Ensure the UI does not interpret the same
//       long-press as a regular button event.
// =============================================================================

class ShippingMode {
public:
    ShippingMode();

    /// Configure BOOT_BUTTON_PIN as INPUT_PULLUP.  Call once from setup().
    void begin();

    /// Poll the button state and update the hold timer.
    /// Must be called every loop() iteration.  Non-blocking — no delay().
    void update();

    /// Returns true once the button has been held for SHIPPING_MODE_HOLD_MS.
    bool shouldEnterSleep() const { return _sleepRequested; }

    /// Clear the sleep-requested flag (call after entering deep sleep or
    /// completing the LED blink sequence).
    void reset();

private:
    bool          _sleepRequested;   // flag: threshold reached
    bool          _buttonDown;       // debounced button state
    unsigned long _pressStartMs;     // millis() when button was first seen LOW
    unsigned long _lastChangeMs;     // last raw-state change (for debounce)
    bool          _lastRawState;     // last raw digitalRead value
    static const unsigned long DEBOUNCE_MS = 50;
};

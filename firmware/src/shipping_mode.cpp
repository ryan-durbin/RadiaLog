#include "shipping_mode.h"

ShippingMode::ShippingMode()
    : _sleepRequested(false)
    , _buttonDown(false)
    , _pressStartMs(0)
    , _lastChangeMs(0)
    , _lastRawState(HIGH)   // idle (not pressed)
{
}

void ShippingMode::begin() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    _sleepRequested = false;
    _buttonDown     = false;
    _pressStartMs   = 0;
    _lastChangeMs   = millis();
    _lastRawState   = digitalRead(BOOT_BUTTON_PIN);
}

void ShippingMode::update() {
    if (_sleepRequested) return;   // already triggered — nothing to do

    unsigned long now = millis();
    bool rawState = digitalRead(BOOT_BUTTON_PIN);   // LOW when pressed

    // --- Debounce ---
    if (rawState != _lastRawState) {
        _lastChangeMs = now;
        _lastRawState = rawState;
    }

    if ((now - _lastChangeMs) < DEBOUNCE_MS) {
        return;   // still bouncing — ignore
    }

    bool pressed = (rawState == LOW);

    // --- State transitions ---
    if (pressed && !_buttonDown) {
        // Button just pressed (after debounce)
        _buttonDown   = true;
        _pressStartMs = now;
    } else if (!pressed && _buttonDown) {
        // Button released before threshold
        _buttonDown   = false;
        _pressStartMs = 0;
    }

    // --- Threshold check ---
    if (_buttonDown && (now - _pressStartMs >= SHIPPING_MODE_HOLD_MS)) {
        _sleepRequested = true;
    }
}

void ShippingMode::reset() {
    _sleepRequested = false;
    _buttonDown     = false;
    _pressStartMs   = 0;
}

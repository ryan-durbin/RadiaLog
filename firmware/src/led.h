#pragma once

#include <Arduino.h>
#include "config.h"

// LED pattern definitions
enum class LedPattern {
    SOLID,        // LED always on — all good
    SLOW_BLINK,   // 1s on / 1s off — no GPS fix
    FAST_BLINK,   // 100ms on / 100ms off — uploading
    DOUBLE_FLASH, // two quick flashes then 1s pause — error
    OFF           // LED off — deep sleep
};

class Led {
public:
    Led();

    // Initialize LED pin — call once from setup()
    void begin();

    // Set the current LED pattern
    void setPattern(LedPattern pattern);

    // Get the current LED pattern
    LedPattern getPattern() const { return _pattern; }

    // Update LED state — call from loop() every iteration
    void update();

private:
    LedPattern _pattern;
    unsigned long _lastToggle;
    bool _ledState;

    // DOUBLE_FLASH sub-state tracking
    uint8_t _flashCount;  // how many flashes done in current cycle
    bool _inPause;        // currently in pause phase

    void _setLed(bool on);
};

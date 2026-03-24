#include "led.h"

// Timing constants (milliseconds)
static const unsigned long SLOW_BLINK_PERIOD  = 1000;  // 1s on / 1s off
static const unsigned long FAST_BLINK_PERIOD  = 100;   // 100ms on / 100ms off
static const unsigned long DOUBLE_FLASH_ON    = 80;    // flash on duration
static const unsigned long DOUBLE_FLASH_OFF   = 100;   // gap between flashes
static const unsigned long DOUBLE_FLASH_PAUSE = 1000;  // pause after two flashes

Led::Led()
    : _pattern(LedPattern::OFF)
    , _lastToggle(0)
    , _ledState(false)
    , _flashCount(0)
    , _inPause(false)
{
}

void Led::begin() {
    pinMode(LED_PIN, OUTPUT);
    _setLed(false);
}

void Led::setPattern(LedPattern pattern) {
    if (_pattern == pattern) return;
    _pattern = pattern;
    _lastToggle = millis();
    _ledState = false;
    _flashCount = 0;
    _inPause = false;

    // Apply immediate state for simple patterns
    if (pattern == LedPattern::SOLID) {
        _setLed(true);
    } else {
        _setLed(false);
    }
}

void Led::update() {
    unsigned long now = millis();

    switch (_pattern) {

        case LedPattern::OFF:
            _setLed(false);
            break;

        case LedPattern::SOLID:
            _setLed(true);
            break;

        case LedPattern::SLOW_BLINK:
            if (now - _lastToggle >= SLOW_BLINK_PERIOD) {
                _ledState = !_ledState;
                _setLed(_ledState);
                _lastToggle = now;
            }
            break;

        case LedPattern::FAST_BLINK:
            if (now - _lastToggle >= FAST_BLINK_PERIOD) {
                _ledState = !_ledState;
                _setLed(_ledState);
                _lastToggle = now;
            }
            break;

        case LedPattern::DOUBLE_FLASH: {
            unsigned long elapsed = now - _lastToggle;

            if (_inPause) {
                // Waiting in long pause after two flashes
                if (elapsed >= DOUBLE_FLASH_PAUSE) {
                    // Start next cycle
                    _inPause = false;
                    _flashCount = 0;
                    _setLed(true);  // first flash on
                    _ledState = true;
                    _lastToggle = now;
                }
            } else {
                // Flash phase: _flashCount tracks completed ON/OFF pairs
                // State machine: ON → OFF → ON → OFF → PAUSE
                if (_ledState) {
                    // Currently ON — wait for ON duration
                    if (elapsed >= DOUBLE_FLASH_ON) {
                        _setLed(false);
                        _ledState = false;
                        _lastToggle = now;
                    }
                } else {
                    // Currently OFF — wait for OFF gap or start pause
                    unsigned long waitTime = (_flashCount >= 2) ? DOUBLE_FLASH_PAUSE : DOUBLE_FLASH_OFF;

                    if (elapsed >= waitTime) {
                        _flashCount++;
                        if (_flashCount >= 2) {
                            // Two flashes done — enter pause
                            _inPause = true;
                            _flashCount = 0;
                            _setLed(false);
                            _lastToggle = now;
                        } else {
                            // Another flash
                            _setLed(true);
                            _ledState = true;
                            _lastToggle = now;
                        }
                    }
                }
            }
            break;
        }
    }
}

void Led::_setLed(bool on) {
    _ledState = on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

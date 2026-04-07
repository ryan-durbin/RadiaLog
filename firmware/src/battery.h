#pragma once
#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// RadiaLog Firmware - Battery Voltage Monitor
// Reads LiPo voltage via ADC through external voltage divider.
// Requires hardware: BAT+ → R1(200k) → ADC_PIN → R2(200k) → GND
// =============================================================================

class Battery {
public:
    Battery();

    /// Initialize ADC pin with 11dB attenuation (0-2.5V range).
    void begin();

    /// Read battery voltage (oversampled). Call periodically from loop().
    void update();

    /// Battery voltage in volts (e.g. 3.85).
    float getVoltage() const { return _voltage; }

    /// Battery voltage in millivolts.
    uint16_t getMillivolts() const { return static_cast<uint16_t>(_voltage * 1000.0f); }

    /// Battery percentage (0-100), linear between EMPTY and FULL thresholds.
    uint8_t getPercent() const;

    /// True if battery voltage reads above 2.0V (divider is connected).
    bool isPresent() const { return _voltage > 2.0f; }

    /// True if voltage suggests USB charging (above fully-charged threshold).
    bool isCharging() const { return _voltage > 4.30f; }

private:
    float _voltage;
};

#endif // BATTERY_H

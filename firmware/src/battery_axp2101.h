#pragma once
#ifndef BATTERY_AXP2101_H
#define BATTERY_AXP2101_H

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// RadiaLog Firmware - AXP2101 PMU Battery Monitor
// Reads battery voltage, percentage, and charging state from the AXP2101
// power management IC over I2C. Used on Waveshare ESP32-S3-Touch-AMOLED-1.75-G.
// =============================================================================

// Forward-declare XPowersAXP2101 to keep the header lightweight
class XPowersAXP2101;

class BatteryAXP2101 {
public:
    BatteryAXP2101();
    ~BatteryAXP2101();

    /// Initialize AXP2101 on the given I2C bus.
    void begin(TwoWire& wire, int sdaPin, int sclPin);

    /// Refresh readings from the PMU. Call periodically from loop().
    void update();

    /// Battery voltage in volts (e.g. 3.85).
    float getVoltage() const { return _voltage; }

    /// Battery voltage in millivolts.
    uint16_t getMillivolts() const { return static_cast<uint16_t>(_voltage * 1000.0f); }

    /// Battery percentage (0-100) from the PMU's fuel gauge.
    uint8_t getPercent() const { return _percent; }

    /// True if a battery is connected.
    bool isPresent() const { return _present; }

    /// True if the battery is currently charging.
    bool isCharging() const { return _charging; }

private:
    XPowersAXP2101* _pmu;
    float   _voltage;
    uint8_t _percent;
    bool    _present;
    bool    _charging;
};

#endif // BATTERY_AXP2101_H

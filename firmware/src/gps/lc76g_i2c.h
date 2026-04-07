#pragma once
#ifndef LC76G_I2C_H
#define LC76G_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include "gps.h"

// =============================================================================
// RadiaLog Firmware - Quectel LC76G GPS Driver (I2C)
// Multi-constellation GNSS (GPS/GLONASS/Galileo/BDS) over I2C.
// Used on Waveshare ESP32-S3-Touch-AMOLED-1.75-G and similar boards.
// =============================================================================

class LC76G_I2C : public GPS {
public:
    /// @param wire     I2C bus (typically Wire)
    /// @param sdaPin   I2C SDA pin
    /// @param sclPin   I2C SCL pin
    /// @param addr     7-bit I2C address for reading NMEA (default 0x10)
    LC76G_I2C(TwoWire& wire, int sdaPin, int sclPin, uint8_t addr = 0x10);

    void begin() override;
    bool poll() override;
    bool hasFix() const override;
    double getLat() const override;
    double getLon() const override;
    float getAlt() const override;
    float getSpeed() const override;
    float getHeading() const override;
    float getAccuracy() const override;
    int getSatellites() const override;

    bool hasValidTime() const override;
    uint16_t getYear() const override;
    uint8_t  getMonth() const override;
    uint8_t  getDay() const override;
    uint8_t  getHour() const override;
    uint8_t  getMinute() const override;
    uint8_t  getSecond() const override;

private:
    TwoWire&    _wire;
    mutable TinyGPSPlus _gps;
    int         _sdaPin;
    int         _sclPin;
    uint8_t     _addr;
    bool        _i2cStarted;
    bool        _hasFix;
    double      _lat;
    double      _lon;
    float       _alt;
    float       _speed;
    float       _heading;
    float       _accuracy;
    int         _satellites;
};

#endif // LC76G_I2C_H

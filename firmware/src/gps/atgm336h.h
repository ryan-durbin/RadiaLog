#pragma once
#ifndef ATGM336H_H
#define ATGM336H_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "gps.h"

// =============================================================================
// RadiaLog Firmware - ATGM336H GPS Driver
// UART-based GPS module driver implementing the GPS abstract interface.
// Uses HardwareSerial (UART1) at 9600 baud, parsed via TinyGPSPlus.
// =============================================================================

class ATGM336H : public GPS {
public:
    /// @param serial  Hardware serial port to use (e.g. Serial1)
    /// @param txPin   TX pin (connects to GPS module's RX)
    /// @param rxPin   RX pin (connects to GPS module's TX)
    /// @param baud    GPS baud rate (default 9600)
    ATGM336H(HardwareSerial& serial, int txPin, int rxPin, uint32_t baud = 9600);

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

    void injectTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t min, uint8_t sec) override;
    void injectPosition(double lat, double lon, float altM) override;

private:
    void _sendCommand(const String& cmd);
    HardwareSerial& _serial;
    TinyGPSPlus     _gps;
    int      _txPin;
    int      _rxPin;
    uint32_t _baud;
    bool     _timeInjected;
    bool     _posInjected;
    bool     _hasFix;
    double   _lat;
    double   _lon;
    float    _alt;
    float    _speed;
    float    _heading;
    float    _accuracy;
    int      _satellites;
};

#endif // ATGM336H_H

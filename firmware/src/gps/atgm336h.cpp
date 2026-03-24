#include "atgm336h.h"

// =============================================================================
// RadiaLog Firmware - ATGM336H GPS Driver Stub Implementation
// Real implementation uses TinyGPSPlus to parse NMEA sentences.
// =============================================================================

ATGM336H::ATGM336H(HardwareSerial& serial, int txPin, int rxPin, uint32_t baud)
    : _serial(serial)
    , _txPin(txPin)
    , _rxPin(rxPin)
    , _baud(baud)
    , _hasFix(false)
    , _lat(0.0)
    , _lon(0.0)
    , _alt(0.0f)
    , _speed(0.0f)
    , _heading(0.0f)
    , _accuracy(0.0f)
    , _satellites(0)
{
}

void ATGM336H::begin() {
    // Stub: no UART initialization
    // Real implementation:
    //   _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
}

bool ATGM336H::poll() {
    // Stub: no NMEA parsing — always returns false (no fix)
    // Real implementation: read from _serial, feed to TinyGPSPlus
    return false;
}

bool ATGM336H::hasFix() const {
    return _hasFix;
}

double ATGM336H::getLat() const {
    return _lat;
}

double ATGM336H::getLon() const {
    return _lon;
}

float ATGM336H::getAlt() const {
    return _alt;
}

float ATGM336H::getSpeed() const {
    return _speed;
}

float ATGM336H::getHeading() const {
    return _heading;
}

float ATGM336H::getAccuracy() const {
    return _accuracy;
}

int ATGM336H::getSatellites() const {
    return _satellites;
}

#include "atgm336h.h"

// =============================================================================
// RadiaLog Firmware - ATGM336H GPS Driver Implementation
// Reads NMEA sentences from UART and parses via TinyGPSPlus.
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
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
}

bool ATGM336H::poll() {
    bool newData = false;

    // Feed all available bytes to TinyGPSPlus
    while (_serial.available() > 0) {
        char c = _serial.read();
        if (_gps.encode(c)) {
            newData = true;
        }
    }

    // Update cached values from parsed NMEA data
    if (_gps.location.isValid() && _gps.location.isUpdated()) {
        _hasFix = true;
        _lat = _gps.location.lat();
        _lon = _gps.location.lng();
    } else if (!_gps.location.isValid()) {
        _hasFix = false;
    }

    if (_gps.altitude.isValid()) {
        _alt = static_cast<float>(_gps.altitude.meters());
    }

    if (_gps.speed.isValid()) {
        _speed = static_cast<float>(_gps.speed.kmph());
    }

    if (_gps.course.isValid()) {
        _heading = static_cast<float>(_gps.course.deg());
    }

    if (_gps.satellites.isValid()) {
        _satellites = _gps.satellites.value();
    }

    // HDOP as accuracy estimate: HDOP * 5m baseline ≈ horizontal accuracy
    if (_gps.hdop.isValid() && _gps.hdop.hdop() > 0.0) {
        _accuracy = static_cast<float>(_gps.hdop.hdop() * 5.0);
    }

    return newData;
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

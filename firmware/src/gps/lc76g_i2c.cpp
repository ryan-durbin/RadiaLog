#include "lc76g_i2c.h"

// =============================================================================
// RadiaLog Firmware - Quectel LC76G I2C GPS Driver Implementation
// Reads NMEA sentences over I2C and parses via TinyGPSPlus.
//
// The LC76G continuously outputs NMEA data. Over I2C, we poll by requesting
// bytes; 0xFF/0x00 bytes indicate no data available.
// =============================================================================

static constexpr int    I2C_READ_CHUNK = 255;  // Max bytes per I2C request
static constexpr int    I2C_FREQ       = 100000; // 100 kHz (LC76G max)

LC76G_I2C::LC76G_I2C(TwoWire& wire, int sdaPin, int sclPin, uint8_t addr)
    : _wire(wire)
    , _sdaPin(sdaPin)
    , _sclPin(sclPin)
    , _addr(addr)
    , _i2cStarted(false)
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

void LC76G_I2C::begin() {
    if (!_i2cStarted) {
        _wire.begin(_sdaPin, _sclPin, I2C_FREQ);
        _i2cStarted = true;
    }
}

bool LC76G_I2C::poll() {
    bool newData = false;

    // Request a chunk of NMEA data from the LC76G
    int available = _wire.requestFrom(_addr, (uint8_t)I2C_READ_CHUNK);

    while (_wire.available()) {
        uint8_t c = _wire.read();

        // LC76G pads with 0xFF when no data; skip null bytes too
        if (c == 0xFF || c == 0x00) continue;

        if (_gps.encode(static_cast<char>(c))) {
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

    // HDOP as accuracy estimate: HDOP * 5m baseline
    if (_gps.hdop.isValid() && _gps.hdop.hdop() > 0.0) {
        _accuracy = static_cast<float>(_gps.hdop.hdop() * 5.0);
    }

    return newData;
}

bool   LC76G_I2C::hasFix()    const { return _hasFix; }
double LC76G_I2C::getLat()     const { return _lat; }
double LC76G_I2C::getLon()     const { return _lon; }
float  LC76G_I2C::getAlt()     const { return _alt; }
float  LC76G_I2C::getSpeed()   const { return _speed; }
float  LC76G_I2C::getHeading() const { return _heading; }
float  LC76G_I2C::getAccuracy()const { return _accuracy; }
int    LC76G_I2C::getSatellites() const { return _satellites; }

bool LC76G_I2C::hasValidTime() const {
    return _gps.date.isValid() && _gps.time.isValid() && _gps.date.year() >= 2024;
}

uint16_t LC76G_I2C::getYear()   const { return _gps.date.year(); }
uint8_t  LC76G_I2C::getMonth()  const { return _gps.date.month(); }
uint8_t  LC76G_I2C::getDay()    const { return _gps.date.day(); }
uint8_t  LC76G_I2C::getHour()   const { return _gps.time.hour(); }
uint8_t  LC76G_I2C::getMinute() const { return _gps.time.minute(); }
uint8_t  LC76G_I2C::getSecond() const { return _gps.time.second(); }

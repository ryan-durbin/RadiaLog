#include "atgm336h.h"
#include "../config.h"
#include "driver/gpio.h"

// =============================================================================
// RadiaLog Firmware - ATGM336H GPS Driver Implementation
// Reads NMEA sentences from UART and parses via TinyGPSPlus.
// =============================================================================

ATGM336H::ATGM336H(HardwareSerial& serial, int txPin, int rxPin, uint32_t baud)
    : _serial(serial)
    , _txPin(txPin)
    , _rxPin(rxPin)
    , _baud(baud)
    , _timeInjected(false)
    , _posInjected(false)
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

#ifdef GPS_POWER_PIN_COUNT
static int _gps_power_pin(int idx) {
    switch (idx) {
        case 0: return GPS_POWER_PIN_0;
        case 1: return GPS_POWER_PIN_1;
        case 2: return GPS_POWER_PIN_2;
        case 3: return GPS_POWER_PIN_3;
    }
    return -1;
}
#endif

void ATGM336H::begin() {
#ifdef GPS_POWER_PIN_COUNT
    for (int i = 0; i < GPS_POWER_PIN_COUNT; i++) {
        pinMode(_gps_power_pin(i), OUTPUT);
        digitalWrite(_gps_power_pin(i), HIGH);
    }
    delay(50);   // Allow the GPS RF/frontend to come up before opening UART
#elif defined(GPS_POWER_PIN)
    // Release any deep-sleep pad hold set by a prior shutdown() so we can
    // drive the pin again on this boot.
    gpio_hold_dis((gpio_num_t)GPS_POWER_PIN);
    pinMode(GPS_POWER_PIN, OUTPUT);
    digitalWrite(GPS_POWER_PIN, HIGH);
    delay(50);   // Allow the GPS RF/frontend to come up before opening UART
#endif
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
}

void ATGM336H::shutdown() {
    // Try several candidate sleep commands — the AT6558 inside the ATGM336H
    // has inconsistent documentation and implementations across firmware
    // revisions, so we fire each one speculatively. Whichever the module
    // actually recognises will drop it into a low-power state; the rest are
    // harmless no-ops. Remove commands that are confirmed unused later.
    const char* const sleepCmds[] = {
        "$PMTK161,0*28\r\n",   // MTK "StandbyMode"
        "$PMTK225,4*2F\r\n",   // MTK "Backup Mode"
        "$PCAS12,0*1E\r\n",    // AT6558 candidate standby
    };
    for (const char* cmd : sleepCmds) {
        _serial.print(cmd);
        _serial.flush();
        delay(30);
    }
    delay(100);        // give the module a moment to act on whichever stuck
    _serial.end();

#ifdef GPS_POWER_PIN_COUNT
    for (int i = 0; i < GPS_POWER_PIN_COUNT; i++) {
        digitalWrite(_gps_power_pin(i), LOW);
    }
    // Latch all pads LOW through deep sleep. Without this the IO drivers
    // power down when the ESP32 sleeps, the pins go high-Z, and the
    // ATGM336H's internal pullup pulls the VCC rail HIGH — which would
    // re-enable the GPS and burn ~25 mA while the MCU is asleep.
    for (int i = 0; i < GPS_POWER_PIN_COUNT; i++) {
        gpio_hold_en((gpio_num_t)_gps_power_pin(i));
    }
    gpio_deep_sleep_hold_en();
#elif defined(GPS_POWER_PIN)
    digitalWrite(GPS_POWER_PIN, LOW);
    // Latch the pad LOW through deep sleep. Without this the IO driver
    // powers down when the ESP32 sleeps, the pin goes high-Z, and the
    // ATGM336H's internal pullup pulls the ON/OFF line HIGH — which would
    // re-enable the GPS and burn ~25 mA while the MCU is asleep.
    gpio_hold_en((gpio_num_t)GPS_POWER_PIN);
    gpio_deep_sleep_hold_en();
#endif
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

bool ATGM336H::hasValidTime() const {
    return _gps.date.isValid() && _gps.time.isValid() && _gps.date.year() >= 2024;
}

uint16_t ATGM336H::getYear() const { return _gps.date.year(); }
uint8_t  ATGM336H::getMonth() const { return _gps.date.month(); }
uint8_t  ATGM336H::getDay() const { return _gps.date.day(); }
uint8_t  ATGM336H::getHour() const { return _gps.time.hour(); }
uint8_t  ATGM336H::getMinute() const { return _gps.time.minute(); }
uint8_t  ATGM336H::getSecond() const { return _gps.time.second(); }

// =============================================================================
// GPS parsing health stats (from TinyGPSPlus)
// =============================================================================

uint32_t ATGM336H::sentencesWithFix() const { return _gps.sentencesWithFix(); }
uint32_t ATGM336H::failedChecksums() const { return _gps.failedChecksum(); }
uint32_t ATGM336H::charsProcessed() const { return _gps.charsProcessed(); }

// =============================================================================
// A-GPS: time and position aiding via PCAS commands
// =============================================================================

void ATGM336H::_sendCommand(const String& cmd) {
    // Compute NMEA XOR checksum over chars between $ and *
    uint8_t cksum = 0;
    for (size_t i = 1; i < cmd.length(); i++) {
        cksum ^= static_cast<uint8_t>(cmd[i]);
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "*%02X\r\n", cksum);
    _serial.print(cmd);
    _serial.print(buf);
}

void ATGM336H::injectTime(uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t min, uint8_t sec) {
    if (_timeInjected) return;

    // $PCAS10,<year>,<month>,<day>,<hour>,<min>,<sec>
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "$PCAS10,%u,%u,%u,%u,%u,%u",
             year, month, day, hour, min, sec);
    _sendCommand(String(cmd));
    _timeInjected = true;
}

void ATGM336H::injectPosition(double lat, double lon, float altM) {
    if (_posInjected) return;

    // $PCAS11,<lat>,<lon>,<alt>  (decimal degrees, meters)
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "$PCAS11,%.6f,%.6f,%.1f", lat, lon, altM);
    _sendCommand(String(cmd));
    _posInjected = true;
}

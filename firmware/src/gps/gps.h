#pragma once
#ifndef GPS_H
#define GPS_H

// =============================================================================
// RadiaLog Firmware - Abstract GPS Interface
// Implemented by ATGM336H (UART) and future GPS modules.
// =============================================================================

class GPS {
public:
    virtual ~GPS() {}

    /// Initialize the GPS hardware (UART, etc.)
    virtual void begin() = 0;

    /// Poll for new NMEA data. Returns true if a new fix is available.
    virtual bool poll() = 0;

    /// Returns true if a valid GPS fix is held.
    virtual bool hasFix() const = 0;

    /// Latitude in decimal degrees (valid only if hasFix())
    virtual double getLat() const = 0;

    /// Longitude in decimal degrees (valid only if hasFix())
    virtual double getLon() const = 0;

    /// Altitude in meters (valid only if hasFix())
    virtual float getAlt() const = 0;

    /// Speed in km/h (valid only if hasFix())
    virtual float getSpeed() const = 0;

    /// Heading in degrees 0-360 (valid only if hasFix())
    virtual float getHeading() const = 0;

    /// Horizontal accuracy in meters (valid only if hasFix())
    virtual float getAccuracy() const = 0;

    /// Number of satellites in use
    virtual int getSatellites() const = 0;

    /// Returns true if the GPS has a valid UTC date+time from NMEA.
    virtual bool hasValidTime() const { return false; }

    /// UTC time components (valid only if hasValidTime())
    virtual uint16_t getYear() const { return 0; }
    virtual uint8_t  getMonth() const { return 0; }
    virtual uint8_t  getDay() const { return 0; }
    virtual uint8_t  getHour() const { return 0; }
    virtual uint8_t  getMinute() const { return 0; }
    virtual uint8_t  getSecond() const { return 0; }

    /// Inject approximate time for A-GPS (UTC). No-op if unsupported.
    virtual void injectTime(uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t min, uint8_t sec) { (void)year; (void)month; (void)day; (void)hour; (void)min; (void)sec; }

    /// Inject approximate position for A-GPS. No-op if unsupported.
    virtual void injectPosition(double lat, double lon, float altM) { (void)lat; (void)lon; (void)altM; }
};

#endif // GPS_H

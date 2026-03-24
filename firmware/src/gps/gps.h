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
};

#endif // GPS_H

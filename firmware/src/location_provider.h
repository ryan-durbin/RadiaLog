#pragma once
#ifndef LOCATION_PROVIDER_H
#define LOCATION_PROVIDER_H

#include <Arduino.h>
#include "gps/gps.h"

// =============================================================================
// RadiaLog Firmware - Location Provider
// Resolves device position via fallback chain: GPS → cached → WiFi geolocation.
// Cache is cleared on boot; expires after 2 minutes of no updates.
// =============================================================================

class LocationProvider {
public:
    enum class Source { NONE, GPS, CACHED, WIFI_GEO };

    LocationProvider();

    /// Call once in setup(). Clears all cached state.
    void begin(const String& googleApiKey);

    /// Call every loop iteration after gps.poll().
    /// Returns true if a valid location is available.
    bool update(GPS& gps, bool wifiStaConnected);

    double getLat() const;
    double getLon() const;
    float  getAlt() const;
    float  getSpeed() const;
    float  getHeading() const;
    float  getAccuracy() const;
    Source getSource() const;

private:
    // Current resolved position
    double _lat, _lon;
    float  _alt, _speed, _heading, _accuracy;
    Source  _source;

    // Cached last-known position
    double   _cachedLat, _cachedLon;
    float    _cachedAlt, _cachedSpeed, _cachedHeading, _cachedAccuracy;
    unsigned long _cachedAtMs;
    bool     _cacheValid;

    static constexpr unsigned long CACHE_EXPIRY_MS    = 120000UL;  // 2 minutes
    static constexpr unsigned long WIFI_GEO_COOLDOWN_MS = 30000UL; // 30s between attempts

    // WiFi geolocation
    String _googleApiKey;
    unsigned long _lastWifiGeoAttemptMs;
    bool _tryWifiGeolocation();

    void _updateCache(double lat, double lon, float alt, float speed, float heading, float accuracy);
};

#endif // LOCATION_PROVIDER_H

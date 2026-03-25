#pragma once
#ifndef LOCATION_PROVIDER_H
#define LOCATION_PROVIDER_H

#include <Arduino.h>
#include "gps/gps.h"

// =============================================================================
// RadiaLog Firmware - Location Provider
// Resolves device position via fallback chain: GPS → stored → WiFi geolocation.
// Last known position is persisted to LittleFS and used for GPS aiding on boot.
// WiFi geolocation only fires when the WiFi environment changes (new BSSIDs).
// =============================================================================

class LocationProvider {
public:
    enum class Source { NONE, GPS, STORED, WIFI_GEO };

    LocationProvider();

    /// Call once in setup().
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

    static constexpr unsigned long WIFI_GEO_COOLDOWN_MS  = 30000UL;  // min interval between API calls

    // WiFi geolocation
    String _googleApiKey;
    unsigned long _lastWifiGeoAttemptMs;
    bool _tryWifiGeolocation();

    // Persistent position (survives reboots, used for GPS aiding + initial position)
    bool _loadStoredPosition();
    void _saveStoredPosition(double lat, double lon, float alt, float accuracy);
    bool _storedValid;
    double _storedLat, _storedLon;
    float  _storedAlt, _storedAccuracy;

    // WiFi environment fingerprint — only re-call API when BSSIDs change
    String _lastWifiFingerprintHash;
    String _computeWifiFingerprint();

    // A-GPS aiding state
    bool _aidingInjected;
};

#endif // LOCATION_PROVIDER_H

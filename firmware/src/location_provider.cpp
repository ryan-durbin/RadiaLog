#include "location_provider.h"
#include "portal/debug_ws.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =============================================================================
// RadiaLog Firmware - LocationProvider Implementation
// Fallback chain: GPS fix → cached position (≤2 min) → WiFi geolocation.
// =============================================================================

LocationProvider::LocationProvider()
    : _lat(0.0), _lon(0.0)
    , _alt(0.0f), _speed(0.0f), _heading(0.0f), _accuracy(0.0f)
    , _source(Source::NONE)
    , _cachedLat(0.0), _cachedLon(0.0)
    , _cachedAlt(0.0f), _cachedSpeed(0.0f), _cachedHeading(0.0f), _cachedAccuracy(0.0f)
    , _cachedAtMs(0)
    , _cacheValid(false)
    , _lastWifiGeoAttemptMs(0)
{
}

void LocationProvider::begin(const String& googleApiKey) {
    _googleApiKey = googleApiKey;

    // Clear all cached state on boot
    _cacheValid = false;
    _cachedLat = 0.0;
    _cachedLon = 0.0;
    _cachedAlt = 0.0f;
    _cachedSpeed = 0.0f;
    _cachedHeading = 0.0f;
    _cachedAccuracy = 0.0f;
    _cachedAtMs = 0;

    _source = Source::NONE;
    _lastWifiGeoAttemptMs = 0;
}

bool LocationProvider::update(GPS& gps, bool wifiStaConnected) {
    // --- Priority 1: Live GPS fix ---
    if (gps.hasFix()) {
        _lat      = gps.getLat();
        _lon      = gps.getLon();
        _alt      = gps.getAlt();
        _speed    = gps.getSpeed();
        _heading  = gps.getHeading();
        _accuracy = gps.getAccuracy();
        _source   = Source::GPS;

        _updateCache(_lat, _lon, _alt, _speed, _heading, _accuracy);
        return true;
    }

    // --- Priority 2: Cached position (within 2 minutes) ---
    if (_cacheValid && (millis() - _cachedAtMs) < CACHE_EXPIRY_MS) {
        _lat      = _cachedLat;
        _lon      = _cachedLon;
        _alt      = _cachedAlt;
        _speed    = _cachedSpeed;
        _heading  = _cachedHeading;
        _accuracy = _cachedAccuracy;
        _source   = Source::CACHED;
        return true;
    }

    // Cache has expired
    if (_cacheValid) {
        _cacheValid = false;
        debugWS.log(MOD_GPS, LVL_INFO, "[Location] Cache expired (>2 min stale).");
    }

    // --- Priority 3: WiFi geolocation ---
    if (wifiStaConnected && _googleApiKey.length() > 0) {
        // Rate limit: only attempt every 30 seconds
        if ((millis() - _lastWifiGeoAttemptMs) >= WIFI_GEO_COOLDOWN_MS) {
            if (_tryWifiGeolocation()) {
                _source = Source::WIFI_GEO;
                _updateCache(_lat, _lon, _alt, _speed, _heading, _accuracy);
                return true;
            }
        }
    }

    // --- No valid location available ---
    _source = Source::NONE;
    return false;
}

void LocationProvider::_updateCache(double lat, double lon, float alt,
                                     float speed, float heading, float accuracy) {
    _cachedLat      = lat;
    _cachedLon      = lon;
    _cachedAlt      = alt;
    _cachedSpeed    = speed;
    _cachedHeading  = heading;
    _cachedAccuracy = accuracy;
    _cachedAtMs     = millis();
    _cacheValid     = true;
}

// =============================================================================
// WiFi Geolocation — Google Geolocation API
// Scans nearby APs, posts MAC/RSSI to Google, returns lat/lon/accuracy.
// =============================================================================

bool LocationProvider::_tryWifiGeolocation() {
    _lastWifiGeoAttemptMs = millis();

    debugWS.log(MOD_GPS, LVL_INFO, "[Location] Attempting WiFi geolocation...");

    // Scan for nearby access points (synchronous, blocks ~2-3s)
    int n = WiFi.scanNetworks(false, false, false, 300);
    if (n <= 0) {
        WiFi.scanDelete();
        debugWS.log(MOD_GPS, LVL_WARN, "[Location] WiFi scan found 0 APs.");
        return false;
    }

    // Build request JSON with up to 10 strongest APs
    JsonDocument doc;
    JsonArray aps = doc["wifiAccessPoints"].to<JsonArray>();

    // Cap at 10 APs
    int count = (n > 10) ? 10 : n;
    for (int i = 0; i < count; i++) {
        JsonObject ap = aps.add<JsonObject>();
        ap["macAddress"]    = WiFi.BSSIDstr(i);
        ap["signalStrength"] = WiFi.RSSI(i);
        ap["channel"]       = WiFi.channel(i);
    }

    WiFi.scanDelete();

    String payload;
    serializeJson(doc, payload);

    // POST to Google Geolocation API
    HTTPClient http;
    String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + _googleApiKey;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    int httpCode = http.POST(payload);

    if (httpCode != HTTP_CODE_OK) {
        debugWS.log(MOD_GPS, LVL_WARN,
            "[Location] WiFi geo HTTP error: " + String(httpCode));
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    // Parse response: { "location": { "lat": ..., "lng": ... }, "accuracy": ... }
    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, response);
    if (err) {
        debugWS.log(MOD_GPS, LVL_WARN, "[Location] WiFi geo JSON parse error.");
        return false;
    }

    if (!respDoc["location"].is<JsonObject>()) {
        debugWS.log(MOD_GPS, LVL_WARN, "[Location] WiFi geo: no location in response.");
        return false;
    }

    _lat      = respDoc["location"]["lat"].as<double>();
    _lon      = respDoc["location"]["lng"].as<double>();
    _accuracy = respDoc["accuracy"].as<float>();
    _alt      = 0.0f;   // WiFi geo does not provide altitude
    _speed    = 0.0f;
    _heading  = 0.0f;

    debugWS.log(MOD_GPS, LVL_INFO,
        "[Location] WiFi geo OK: " + String(_lat, 5) + ", " + String(_lon, 5)
        + " (±" + String(_accuracy, 0) + "m)");

    return true;
}

// --- Getters -----------------------------------------------------------------

double LocationProvider::getLat() const { return _lat; }
double LocationProvider::getLon() const { return _lon; }
float  LocationProvider::getAlt() const { return _alt; }
float  LocationProvider::getSpeed() const { return _speed; }
float  LocationProvider::getHeading() const { return _heading; }
float  LocationProvider::getAccuracy() const { return _accuracy; }
LocationProvider::Source LocationProvider::getSource() const { return _source; }

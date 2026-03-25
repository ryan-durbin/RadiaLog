#include "location_provider.h"
#include "portal/debug_ws.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// =============================================================================
// RadiaLog Firmware - LocationProvider Implementation
// Fallback chain: GPS fix → in-memory cache (≤2 min) → stored position → WiFi geo.
// Persists last known position to LittleFS for GPS aiding and boot-time use.
// Only calls Google Geolocation API when WiFi environment changes.
// =============================================================================

static const char* STORED_POS_PATH = "/last_pos.json";

LocationProvider::LocationProvider()
    : _lat(0.0), _lon(0.0)
    , _alt(0.0f), _speed(0.0f), _heading(0.0f), _accuracy(0.0f)
    , _source(Source::NONE)
    , _lastWifiGeoAttemptMs(0)
    , _storedValid(false)
    , _storedLat(0.0), _storedLon(0.0)
    , _storedAlt(0.0f), _storedAccuracy(0.0f)
    , _aidingInjected(false)
{
}

void LocationProvider::begin(const String& googleApiKey) {
    _googleApiKey = googleApiKey;
    _source = Source::NONE;
    _lastWifiGeoAttemptMs = 0;
    _aidingInjected = false;

    // Load last known position from LittleFS
    if (_loadStoredPosition()) {
        debugWS.log(MOD_GPS, LVL_INFO,
            "[Location] Stored position loaded: " + String(_storedLat, 5)
            + ", " + String(_storedLon, 5) + " (±" + String(_storedAccuracy, 0) + "m)");
    } else {
        debugWS.log(MOD_GPS, LVL_INFO, "[Location] No stored position found.");
    }
}

bool LocationProvider::update(GPS& gps, bool wifiStaConnected) {
    // --- A-GPS aiding: inject time + position once after NTP sync ---
    if (!_aidingInjected && _storedValid && time(nullptr) > 1000000000) {
        struct tm t;
        time_t now = time(nullptr);
        gmtime_r(&now, &t);
        gps.injectTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                       t.tm_hour, t.tm_min, t.tm_sec);
        gps.injectPosition(_storedLat, _storedLon, _storedAlt);
        _aidingInjected = true;
        debugWS.log(MOD_GPS, LVL_INFO, "[Location] A-GPS aiding injected.");
    }

    // --- Priority 1: Live GPS fix (the only truly trustworthy source) ---
    if (gps.hasFix()) {
        _lat      = gps.getLat();
        _lon      = gps.getLon();
        _alt      = gps.getAlt();
        _speed    = gps.getSpeed();
        _heading  = gps.getHeading();
        _accuracy = gps.getAccuracy();
        _source   = Source::GPS;

        _saveStoredPosition(_lat, _lon, _alt, _accuracy);
        return true;
    }

    // --- Priority 2: WiFi geolocation (stationary/indoor fallback) ---
    //     Only call API when WiFi environment changes (new location).
    if (wifiStaConnected && _googleApiKey.length() > 0) {
        if ((millis() - _lastWifiGeoAttemptMs) >= WIFI_GEO_COOLDOWN_MS) {
            String fp = _computeWifiFingerprint();
            if (fp.length() > 0 && fp != _lastWifiFingerprintHash) {
                if (_tryWifiGeolocation()) {
                    _source = Source::WIFI_GEO;
                    _saveStoredPosition(_lat, _lon, _alt, _accuracy);
                    _lastWifiFingerprintHash = fp;
                    return true;
                }
                _lastWifiFingerprintHash = fp;
            } else if (fp == _lastWifiFingerprintHash && _lastWifiFingerprintHash.length() > 0) {
                // Same WiFi environment as last successful geo — reuse that position
                _lastWifiGeoAttemptMs = millis();
                _source = Source::WIFI_GEO;
                return true;
            }
        } else if (_source == Source::WIFI_GEO && _lastWifiFingerprintHash.length() > 0) {
            // Between cooldowns, keep returning the current WiFi geo position
            return true;
        }
    }

    // --- No fresh position available ---
    // Stored position is only used for A-GPS aiding, never for tagging readings.
    _source = Source::NONE;
    return false;
}

// =============================================================================
// Persistent position — saved to /last_pos.json on LittleFS
// =============================================================================

bool LocationProvider::_loadStoredPosition() {
    File f = LittleFS.open(STORED_POS_PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    _storedLat      = doc["lat"].as<double>();
    _storedLon      = doc["lon"].as<double>();
    _storedAlt      = doc["alt"].as<float>();
    _storedAccuracy = doc["acc"].as<float>();

    // Sanity check
    if (_storedLat == 0.0 && _storedLon == 0.0) return false;

    _storedValid = true;
    return true;
}

void LocationProvider::_saveStoredPosition(double lat, double lon, float alt, float accuracy) {
    // Don't re-save if position hasn't moved significantly (>100m)
    if (_storedValid) {
        double dlat = lat - _storedLat;
        double dlon = lon - _storedLon;
        double distApprox = (dlat * dlat + dlon * dlon) * 111000.0 * 111000.0;
        if (distApprox < 100.0 * 100.0) return;  // <100m, skip write
    }

    _storedLat      = lat;
    _storedLon      = lon;
    _storedAlt      = alt;
    _storedAccuracy = accuracy;
    _storedValid    = true;

    JsonDocument doc;
    doc["lat"] = lat;
    doc["lon"] = lon;
    doc["alt"] = alt;
    doc["acc"] = accuracy;

    File f = LittleFS.open(STORED_POS_PATH, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

// =============================================================================
// WiFi fingerprint — hash of top BSSIDs to detect location change
// =============================================================================

String LocationProvider::_computeWifiFingerprint() {
    // Quick scan (non-blocking results from the system's cached scan)
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        // Kick off a scan for next time
        WiFi.scanNetworks(true, false, false, 300);
        return "";
    }
    if (n == WIFI_SCAN_RUNNING || n <= 0) {
        return "";
    }

    // Sort top 3 BSSIDs by signal strength and hash them
    // Simple approach: concatenate the top 3 MAC addresses
    String fp;
    int count = (n > 3) ? 3 : n;
    for (int i = 0; i < count; i++) {
        fp += WiFi.BSSIDstr(i);
    }
    WiFi.scanDelete();

    // Kick off next scan for future comparisons
    WiFi.scanNetworks(true, false, false, 300);

    return fp;
}

// =============================================================================
// WiFi Geolocation — Google Geolocation API
// =============================================================================

bool LocationProvider::_tryWifiGeolocation() {
    _lastWifiGeoAttemptMs = millis();

    debugWS.log(MOD_GPS, LVL_INFO, "[Location] WiFi geolocation API call...");

    // Synchronous scan for AP data to send to Google
    int n = WiFi.scanNetworks(false, false, false, 300);
    if (n <= 0) {
        WiFi.scanDelete();
        debugWS.log(MOD_GPS, LVL_WARN, "[Location] WiFi scan found 0 APs.");
        return false;
    }

    JsonDocument doc;
    JsonArray aps = doc["wifiAccessPoints"].to<JsonArray>();
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

    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, response);
    if (err || !respDoc["location"].is<JsonObject>()) {
        debugWS.log(MOD_GPS, LVL_WARN, "[Location] WiFi geo JSON parse error.");
        return false;
    }

    _lat      = respDoc["location"]["lat"].as<double>();
    _lon      = respDoc["location"]["lng"].as<double>();
    _accuracy = respDoc["accuracy"].as<float>();
    _alt      = 0.0f;
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

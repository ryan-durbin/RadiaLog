#include "config_mgr.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// =============================================================================
// RadiaLog Firmware - ConfigMgr Implementation
// Reads/writes /config.json on LittleFS using ArduinoJson.
// =============================================================================

static const char* CONFIG_FILE = "/config.json";

ConfigMgr::ConfigMgr()
    : _wifiCount(0)
    , _token("")
    , _uploadUrl("https://radiamaps.com/api/radialog/upload")
    , _deviceId("RadiaLog-0001")
    , _deviceName("RadiaLog")
    , _readingIntervalMs(2000)
    , _apPassword("")
{
}

bool ConfigMgr::load() {
    if (!LittleFS.begin(true)) {
        return false;
    }

    if (!LittleFS.exists(CONFIG_FILE)) {
        // No config file — keep defaults
        return true;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        // Invalid JSON — keep defaults
        return true;
    }

    // WiFi networks
    _wifiCount = 0;
    if (doc["wifi"].is<JsonArray>()) {
        JsonArray wifiArr = doc["wifi"].as<JsonArray>();
        for (JsonObject net : wifiArr) {
            if (_wifiCount >= MAX_WIFI) break;
            _wifiSSID[_wifiCount] = net["ssid"].as<String>();
            _wifiPass[_wifiCount] = net["password"].as<String>();
            _wifiCount++;
        }
    }

    // RadiaMaps settings
    if (doc["radiamaps"].is<JsonObject>()) {
        JsonObject rm = doc["radiamaps"].as<JsonObject>();
        if (rm["token"].is<const char*>())    _token     = rm["token"].as<String>();
        if (rm["url"].is<const char*>())       _uploadUrl = rm["url"].as<String>();
        if (rm["device_id"].is<const char*>()) _deviceId  = rm["device_id"].as<String>();
    }

    // Device settings
    if (doc["device"].is<JsonObject>()) {
        JsonObject dev = doc["device"].as<JsonObject>();
        if (dev["name"].is<const char*>())              _deviceName       = dev["name"].as<String>();
        if (dev["reading_interval_ms"].is<uint32_t>())  _readingIntervalMs = dev["reading_interval_ms"].as<uint32_t>();
    }

    // AP settings
    if (doc["ap"].is<JsonObject>()) {
        JsonObject ap = doc["ap"].as<JsonObject>();
        if (ap["password"].is<const char*>()) _apPassword = ap["password"].as<String>();
    }

    return true;
}

bool ConfigMgr::save() {
    JsonDocument doc;

    // WiFi networks
    JsonArray wifiArr = doc["wifi"].to<JsonArray>();
    for (int i = 0; i < _wifiCount; i++) {
        JsonObject net = wifiArr.add<JsonObject>();
        net["ssid"]     = _wifiSSID[i];
        net["password"] = _wifiPass[i];
    }

    // RadiaMaps settings
    JsonObject rm = doc["radiamaps"].to<JsonObject>();
    rm["token"]     = _token;
    rm["url"]       = _uploadUrl;
    rm["device_id"] = _deviceId;

    // Device settings
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["name"]                = _deviceName;
    dev["reading_interval_ms"] = _readingIntervalMs;

    // AP settings
    JsonObject ap = doc["ap"].to<JsonObject>();
    ap["password"] = _apPassword;

    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();
    return written > 0;
}

// --- Getters ----------------------------------------------------------------

int ConfigMgr::getWifiCount() const {
    return _wifiCount;
}

String ConfigMgr::getWifiSSID(int i) const {
    if (i < 0 || i >= _wifiCount) return "";
    return _wifiSSID[i];
}

String ConfigMgr::getWifiPass(int i) const {
    if (i < 0 || i >= _wifiCount) return "";
    return _wifiPass[i];
}

String ConfigMgr::getToken() const {
    return _token;
}

String ConfigMgr::getUploadUrl() const {
    return _uploadUrl;
}

String ConfigMgr::getDeviceId() const {
    return _deviceId;
}

String ConfigMgr::getDeviceName() const {
    return _deviceName;
}

uint32_t ConfigMgr::getReadingIntervalMs() const {
    return _readingIntervalMs;
}

String ConfigMgr::getApPassword() const {
    return _apPassword;
}

// --- Setters ----------------------------------------------------------------

void ConfigMgr::setWifi(int i, const String& ssid, const String& pass) {
    if (i < 0 || i >= MAX_WIFI) return;
    _wifiSSID[i] = ssid;
    _wifiPass[i] = pass;
    if (i >= _wifiCount) {
        _wifiCount = i + 1;
    }
}

void ConfigMgr::removeWifi(int i) {
    if (i < 0 || i >= _wifiCount) return;
    // Shift remaining entries down
    for (int j = i; j < _wifiCount - 1; j++) {
        _wifiSSID[j] = _wifiSSID[j + 1];
        _wifiPass[j] = _wifiPass[j + 1];
    }
    _wifiCount--;
    _wifiSSID[_wifiCount] = "";
    _wifiPass[_wifiCount] = "";
}

void ConfigMgr::setToken(const String& token) {
    _token = token;
}

void ConfigMgr::setUploadUrl(const String& url) {
    _uploadUrl = url;
}

void ConfigMgr::setDeviceId(const String& id) {
    _deviceId = id;
}

void ConfigMgr::setDeviceName(const String& name) {
    _deviceName = name;
}

void ConfigMgr::setReadingIntervalMs(uint32_t ms) {
    if (ms < 500) ms = 500;      // minimum 500ms
    if (ms > 60000) ms = 60000;  // maximum 60s
    _readingIntervalMs = ms;
}

void ConfigMgr::setApPassword(const String& pass) {
    _apPassword = pass;
}

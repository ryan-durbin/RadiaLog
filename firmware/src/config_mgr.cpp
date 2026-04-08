#include "config_mgr.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// =============================================================================
// RadiaLog Firmware - ConfigMgr Implementation
// Reads/writes /config.json on LittleFS using ArduinoJson.
// Critical settings are also persisted to NVS so they survive flashing.
// =============================================================================

static const char* CONFIG_FILE   = "/config.json";
static const char* NVS_NAMESPACE = "radialog";

static String buildDefaultDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String("RadiaLog-") + suffix;
}

ConfigMgr::ConfigMgr()
    : _wifiCount(0)
    , _token("")
    , _uploadUrl("https://radiamaps.com/api/radialog/upload")
    , _deviceId(buildDefaultDeviceId())
    , _deviceName("RadiaLog")
    , _readingIntervalMs(2000)
    , _apPassword("")
    , _googleApiKey("")
    , _displayTimeoutSec(0)
    , _buttonWakeEnabled(true)
    , _totalReadingsLogged(0)
    , _readingsSinceFlush(0)
{
}

bool ConfigMgr::load() {
    // Try NVS first — these survive firmware flashing
    bool nvsLoaded = loadFromNVS();

    if (!LittleFS.begin(true)) {
        return nvsLoaded;
    }

    // If NVS already had our settings, we're done — NVS is authoritative.
    // Still load config.json for non-NVS fields (BLE devices, button_wake).
    // If NVS was empty, load everything from config.json and seed NVS.

    if (!LittleFS.exists(CONFIG_FILE)) {
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
        return true;
    }

    // --- Fields always loaded from config.json (not in NVS) ---

    // BLE RadiaCode devices
    _bleDeviceMacs.clear();
    if (doc["ble_devices"].is<JsonArray>()) {
        JsonArray bleArr = doc["ble_devices"].as<JsonArray>();
        for (JsonVariant v : bleArr) {
            if (_bleDeviceMacs.size() >= MAX_BLE_DEVICES) break;
            if (v.is<const char*>()) {
                _bleDeviceMacs.push_back(v.as<String>());
            }
        }
    }

    // Display button_wake (not critical enough for NVS)
    if (doc["display"].is<JsonObject>()) {
        JsonObject disp = doc["display"].as<JsonObject>();
        if (disp["button_wake"].is<bool>())
            _buttonWakeEnabled = disp["button_wake"].as<bool>();
    }

    // --- NVS-protected fields: only load from config.json if NVS was empty ---
    if (!nvsLoaded) {
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

        // Geolocation settings
        if (doc["geolocation"].is<JsonObject>()) {
            JsonObject geo = doc["geolocation"].as<JsonObject>();
            if (geo["google_api_key"].is<const char*>())
                _googleApiKey = geo["google_api_key"].as<String>();
        }

        // Display timeout
        if (doc["display"].is<JsonObject>()) {
            JsonObject disp = doc["display"].as<JsonObject>();
            if (disp["timeout_sec"].is<uint16_t>())
                _displayTimeoutSec = disp["timeout_sec"].as<uint16_t>();
        }

        // Seed NVS with the config.json values so they survive future flashes
        saveToNVS();
    }

    return true;
}

bool ConfigMgr::save() {
    // Always persist critical settings to NVS
    saveToNVS();

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

    // Geolocation settings
    JsonObject geo = doc["geolocation"].to<JsonObject>();
    geo["google_api_key"] = _googleApiKey;

    // BLE RadiaCode devices
    JsonArray bleArr = doc["ble_devices"].to<JsonArray>();
    for (const auto& mac : _bleDeviceMacs) {
        bleArr.add(mac);
    }

    // Display settings
    JsonObject disp = doc["display"].to<JsonObject>();
    disp["timeout_sec"] = _displayTimeoutSec;
    disp["button_wake"] = _buttonWakeEnabled;

    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();
    return written > 0;
}

// --- NVS persistence (survives firmware flashing) ---------------------------

bool ConfigMgr::saveToNVS() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
        return false;
    }

    // WiFi networks — store count + indexed keys
    prefs.putInt("wifi_count", _wifiCount);
    for (int i = 0; i < _wifiCount; i++) {
        prefs.putString(("wssid" + String(i)).c_str(), _wifiSSID[i]);
        prefs.putString(("wpass" + String(i)).c_str(), _wifiPass[i]);
    }

    // RadiaMaps
    prefs.putString("token", _token);
    prefs.putString("upload_url", _uploadUrl);
    prefs.putString("device_id", _deviceId);

    // Device
    prefs.putString("dev_name", _deviceName);
    prefs.putUInt("interval_ms", _readingIntervalMs);

    // AP
    prefs.putString("ap_pass", _apPassword);

    // Geolocation
    prefs.putString("google_key", _googleApiKey);

    // Display
    prefs.putUShort("disp_timeout", _displayTimeoutSec);

    // Lifetime counter
    prefs.putULong64("total_rdgs", _totalReadingsLogged);

    // Mark NVS as seeded
    prefs.putBool("seeded", true);

    prefs.end();
    return true;
}

bool ConfigMgr::loadFromNVS() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only
        return false;
    }

    if (!prefs.getBool("seeded", false)) {
        prefs.end();
        return false;  // NVS has never been written — fall back to config.json
    }

    // WiFi networks
    _wifiCount = prefs.getInt("wifi_count", 0);
    if (_wifiCount > MAX_WIFI) _wifiCount = MAX_WIFI;
    for (int i = 0; i < _wifiCount; i++) {
        _wifiSSID[i] = prefs.getString(("wssid" + String(i)).c_str(), "");
        _wifiPass[i] = prefs.getString(("wpass" + String(i)).c_str(), "");
    }

    // RadiaMaps
    _token     = prefs.getString("token", _token);
    _uploadUrl = prefs.getString("upload_url", _uploadUrl);
    _deviceId  = prefs.getString("device_id", _deviceId);

    // Device
    _deviceName       = prefs.getString("dev_name", _deviceName);
    _readingIntervalMs = prefs.getUInt("interval_ms", _readingIntervalMs);

    // AP
    _apPassword = prefs.getString("ap_pass", _apPassword);

    // Geolocation
    _googleApiKey = prefs.getString("google_key", _googleApiKey);

    // Display
    _displayTimeoutSec = prefs.getUShort("disp_timeout", _displayTimeoutSec);

    // Lifetime counter
    _totalReadingsLogged = prefs.getULong64("total_rdgs", 0);

    prefs.end();
    return true;
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

String ConfigMgr::getGoogleApiKey() const {
    return _googleApiKey;
}

void ConfigMgr::setGoogleApiKey(const String& key) {
    _googleApiKey = key;
}

int ConfigMgr::getBleDeviceCount() const {
    return static_cast<int>(_bleDeviceMacs.size());
}

String ConfigMgr::getBleDeviceMac(int i) const {
    if (i < 0 || i >= static_cast<int>(_bleDeviceMacs.size())) return "";
    return _bleDeviceMacs[i];
}

uint16_t ConfigMgr::getDisplayTimeoutSec() const {
    return _displayTimeoutSec;
}

void ConfigMgr::setDisplayTimeoutSec(uint16_t sec) {
    _displayTimeoutSec = sec;
}

bool ConfigMgr::getButtonWakeEnabled() const {
    return _buttonWakeEnabled;
}

void ConfigMgr::setButtonWakeEnabled(bool enabled) {
    _buttonWakeEnabled = enabled;
}

uint64_t ConfigMgr::getTotalReadingsLogged() const {
    return _totalReadingsLogged;
}

void ConfigMgr::setTotalReadingsLogged(uint64_t count) {
    _totalReadingsLogged = count;
    _readingsSinceFlush = 1;  // Mark dirty so flush will write
}

void ConfigMgr::incrementTotalReadingsLogged() {
    _totalReadingsLogged++;
    _readingsSinceFlush++;
    if (_readingsSinceFlush >= FLUSH_INTERVAL) {
        flushTotalReadingsLogged();
    }
}

void ConfigMgr::flushTotalReadingsLogged() {
    if (_readingsSinceFlush == 0) return;
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putULong64("total_rdgs", _totalReadingsLogged);
        prefs.end();
    }
    _readingsSinceFlush = 0;
}


void ConfigMgr::setBleDevices(const std::vector<String>& macs) {
    _bleDeviceMacs.clear();
    for (const auto& mac : macs) {
        if (_bleDeviceMacs.size() >= MAX_BLE_DEVICES) break;
        if (mac.length() > 0) {
            _bleDeviceMacs.push_back(mac);
        }
    }
}

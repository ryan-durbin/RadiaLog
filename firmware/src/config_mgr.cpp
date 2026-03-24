#include "config_mgr.h"

// =============================================================================
// RadiaLog Firmware - ConfigMgr Stub Implementation
// Real implementation reads /config.json from LittleFS.
// This stub returns compile-safe defaults.
// =============================================================================

ConfigMgr::ConfigMgr()
    : _wifiCount(0)
    , _token("")
    , _uploadUrl("http://radialog.local/api/readings")
    , _deviceId("radialog-001")
    , _readingIntervalMs(2000)
{
}

bool ConfigMgr::load() {
    // Stub: no LittleFS read — return defaults
    // Real implementation: open /config.json, parse with ArduinoJson
    return true;
}

bool ConfigMgr::save() {
    // Stub: no LittleFS write
    return true;
}

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

uint32_t ConfigMgr::getReadingIntervalMs() const {
    return _readingIntervalMs;
}

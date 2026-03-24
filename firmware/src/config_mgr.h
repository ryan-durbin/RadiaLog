#pragma once
#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <Arduino.h>

// =============================================================================
// RadiaLog Firmware - Configuration Manager
// Reads /config.json from LittleFS; provides defaults if missing.
// =============================================================================

class ConfigMgr {
public:
    ConfigMgr();

    /// Load configuration from /config.json on LittleFS.
    /// Returns defaults silently if the file is missing or invalid.
    bool load();

    /// Save current configuration to /config.json on LittleFS.
    bool save();

    // --- WiFi credentials ---
    int     getWifiCount() const;
    String  getWifiSSID(int i) const;
    String  getWifiPass(int i) const;
    void    setWifi(int i, const String& ssid, const String& pass);
    void    removeWifi(int i);

    // --- Upload ---
    String  getToken() const;
    void    setToken(const String& token);
    String  getUploadUrl() const;
    void    setUploadUrl(const String& url);
    String  getDeviceId() const;
    void    setDeviceId(const String& id);

    // --- Device ---
    String   getDeviceName() const;
    void     setDeviceName(const String& name);
    uint32_t getReadingIntervalMs() const;
    void     setReadingIntervalMs(uint32_t ms);

    // --- AP ---
    String  getApPassword() const;
    void    setApPassword(const String& pass);

    static constexpr int MAX_WIFI = 4;

private:
    String   _wifiSSID[MAX_WIFI];
    String   _wifiPass[MAX_WIFI];
    int      _wifiCount;
    String   _token;
    String   _uploadUrl;
    String   _deviceId;
    String   _deviceName;
    uint32_t _readingIntervalMs;
    String   _apPassword;
};

#endif // CONFIG_MGR_H

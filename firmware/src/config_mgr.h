#pragma once
#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

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

    // --- Display ---
    uint16_t getDisplayTimeoutSec() const;
    void     setDisplayTimeoutSec(uint16_t sec);
    bool     getButtonWakeEnabled() const;
    void     setButtonWakeEnabled(bool enabled);

    // --- Geolocation ---
    String  getGoogleApiKey() const;
    void    setGoogleApiKey(const String& key);

    // --- BLE RadiaCode devices ---
    int     getBleDeviceCount() const;
    String  getBleDeviceMac(int i) const;
    void    setBleDevices(const std::vector<String>& macs);

    // --- Lifetime reading counter (NVS-persisted, survives firmware updates) ---
    uint64_t getTotalReadingsLogged() const;
    void     setTotalReadingsLogged(uint64_t count);
    void     incrementTotalReadingsLogged();
    void     flushTotalReadingsLogged();   ///< Force-write to NVS now


    /// Save critical settings to NVS (survives firmware flashing).
    bool saveToNVS();

    /// Load critical settings from NVS. Returns true if NVS had data.
    bool loadFromNVS();

    static constexpr int MAX_WIFI = 4;
    static constexpr int MAX_BLE_DEVICES = 4;

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
    String   _googleApiKey;
    std::vector<String> _bleDeviceMacs;
    uint16_t _displayTimeoutSec;
    bool     _buttonWakeEnabled;

    // Lifetime counter — batched NVS writes to reduce flash wear
    uint64_t      _totalReadingsLogged;
    uint32_t      _readingsSinceFlush;   ///< Counts since last NVS write
    static constexpr uint32_t FLUSH_INTERVAL = 100; ///< Write to NVS every N readings
};

#endif // CONFIG_MGR_H

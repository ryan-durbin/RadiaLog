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
    /// Number of configured WiFi networks (0 if none)
    int     getWifiCount() const;
    /// SSID for network at index i (empty string if out of range)
    String  getWifiSSID(int i) const;
    /// Password for network at index i (empty string if out of range)
    String  getWifiPass(int i) const;

    // --- Upload ---
    /// Bearer token for upload API
    String  getToken() const;
    /// Upload endpoint URL
    String  getUploadUrl() const;
    /// Unique device identifier
    String  getDeviceId() const;
    /// Interval between readings in milliseconds
    uint32_t getReadingIntervalMs() const;

private:
    // Stored values
    String   _wifiSSID[4];
    String   _wifiPass[4];
    int      _wifiCount;
    String   _token;
    String   _uploadUrl;
    String   _deviceId;
    uint32_t _readingIntervalMs;
};

#endif // CONFIG_MGR_H

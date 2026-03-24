#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// =============================================================================
// RadiaLog Firmware - WiFi Manager
// Manages AP+STA dual mode (WIFI_AP_STA).
// AP: always broadcasts RadiaLog-XXXX (last 4 hex of MAC)
// STA: connects to configured networks for data upload
// =============================================================================

class WifiMgr {
public:
    WifiMgr();

    /// Initialize WiFi in WIFI_AP_STA mode.
    /// Sets up AP with RadiaLog-XXXX SSID (XXXX = last 4 of MAC).
    /// apPassword: empty string = open network
    void begin(const String& apPassword = "");

    /// Returns true if STA mode has an active internet connection.
    bool isSTAConnected() const;

    /// Returns the STA IP address (or 0.0.0.0 if not connected).
    IPAddress getSTAIP() const;

    /// Returns the AP IP address.
    IPAddress getAPIP() const;

    /// Returns RSSI of current STA connection (dBm), or 0 if not connected.
    int getSignalStrength() const;

    /// Returns current STA SSID, or empty string if not connected.
    String getSSID() const;

private:
    void _setupAP(const String& password);
    String _buildAPSsid();

    bool      _staConnected;
    IPAddress _apIP;
    String    _apSsid;
};

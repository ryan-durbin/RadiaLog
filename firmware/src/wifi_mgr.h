#pragma once
#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <Arduino.h>
#include "config_mgr.h"

// =============================================================================
// RadiaLog Firmware - WiFi Manager
// Manages both AP mode (config portal) and STA mode (upload).
// =============================================================================

class WifiMgr {
public:
    WifiMgr();

    /// Start WiFi: AP mode always on, STA mode if credentials in config.
    void begin(ConfigMgr& cfg);

    /// Returns true if STA mode has an active internet connection.
    bool isStaConnected() const;

    /// Returns RSSI of current STA connection, or 0 if not connected.
    int getStaRSSI() const;

    /// Returns number of clients connected to the AP.
    int getApClients() const;

private:
    bool _staConnected;
    int  _staRSSI;
    int  _apClients;
};

#endif // WIFI_MGR_H

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <functional>
#include "config.h"

// =============================================================================
// RadiaLog Firmware - WiFi Manager
// Manages AP+STA dual mode (WIFI_AP_STA).
// AP: broadcasts RadiaLog-XXXX, auto-disables after AP_AUTO_OFF_MS with no
//     clients connected. Users hit reset to re-enable.
// STA: connects to configured networks for data upload, auto-reconnects.
//     Uses WIFI_PS_MIN_MODEM power saving when STA is connected.
// =============================================================================

/// Credentials for a saved WiFi network.
/// Lower priority value = higher preference (0 = highest).
struct WifiCredentials {
    String ssid;
    String password;
    uint8_t priority;  // 0 = highest priority
};

/// Callback type for STA connect/disconnect events.
using WifiEventCallback = std::function<void()>;

class WifiMgr {
public:
    WifiMgr();

    /// Initialize WiFi in WIFI_AP_STA mode.
    /// Sets up AP with RadiaLog-XXXX SSID (XXXX = last 4 of MAC).
    /// apPassword: empty string = open network
    void begin(const String& apPassword = "");

    /// Update method — call from loop() to handle non-blocking reconnect.
    void update();

    // -------------------------------------------------------------------------
    // Network configuration
    // -------------------------------------------------------------------------

    /// Set up to 3 configured networks. Sorted by priority (lowest value first).
    void setNetworks(const std::vector<WifiCredentials>& networks);

    /// Attempt to connect to the highest-priority available network.
    /// Non-blocking: initiates connection and returns immediately.
    void connectSTA();

    // -------------------------------------------------------------------------
    // Status queries
    // -------------------------------------------------------------------------

    /// Returns true if STA mode has an active connection.
    bool isSTAConnected() const;

    /// Returns true if the AP is currently active.
    bool isAPActive() const;

    /// Returns the STA IP address (or 0.0.0.0 if not connected).
    IPAddress getSTAIP() const;

    /// Returns the AP IP address (valid only while AP is active).
    IPAddress getAPIP() const;

    /// Returns RSSI of current STA connection (dBm), or 0 if not connected.
    int getSignalStrength() const;

    /// Returns current STA SSID, or empty string if not connected.
    String getSSID() const;

    // -------------------------------------------------------------------------
    // Event callbacks
    // -------------------------------------------------------------------------

    /// Register a callback invoked when STA connects successfully.
    void registerOnConnect(WifiEventCallback cb);

    /// Register a callback invoked when STA disconnects.
    void registerOnDisconnect(WifiEventCallback cb);

private:
    void _setupAP(const String& password);
    String _buildAPSsid();

    // WiFi event handler (static, registered with WiFi.onEvent)
    static void _wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

    // Pointer to singleton (needed for static event handler)
    static WifiMgr* _instance;

    // Try next network in priority-sorted list
    void _tryNextNetwork();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    bool      _staConnected;
    IPAddress _apIP;
    String    _apSsid;

    // AP auto-shutdown: disable AP after AP_AUTO_OFF_MS with no clients
    bool          _apActive;
    unsigned long _apLastClientSeen;   // millis() when a client was last connected
    void _checkAPAutoOff();

    // Up to 3 networks, sorted by priority
    static constexpr uint8_t MAX_NETWORKS = 3;
    WifiCredentials _networks[MAX_NETWORKS];
    uint8_t _networkCount;
    uint8_t _currentNetworkIndex;  // index being tried in _networks[]

    // Non-blocking reconnect timing
    unsigned long _reconnectAt;    // millis() timestamp to attempt reconnect
    bool _reconnectPending;

    // Exponential backoff for reconnect (ms)
    unsigned long _reconnectDelay;
    static constexpr unsigned long RECONNECT_DELAY_MIN_MS = 2000UL;
    static constexpr unsigned long RECONNECT_DELAY_MAX_MS = 300000UL; // 5 min

    // Connection attempt timeout
    unsigned long _connectStartedAt;
    bool _connectingNow;
    static constexpr unsigned long CONNECT_TIMEOUT_MS = 15000UL;

    // User callbacks
    WifiEventCallback _onConnectCb;
    WifiEventCallback _onDisconnectCb;
};

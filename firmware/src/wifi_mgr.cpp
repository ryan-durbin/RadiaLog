#include "wifi_mgr.h"
#include <algorithm>

// =============================================================================
// RadiaLog Firmware - WiFi Manager Implementation
// ESP32 WIFI_AP_STA dual mode:
//   AP:  RadiaLog-XXXX always on (XXXX = last 4 hex chars of MAC)
//   STA: connects to configured networks (priority order), auto-reconnects
// =============================================================================

// Static singleton pointer for WiFi event handler
WifiMgr* WifiMgr::_instance = nullptr;

WifiMgr::WifiMgr()
    : _staConnected(false)
    , _apIP()
    , _networkCount(0)
    , _currentNetworkIndex(0)
    , _reconnectAt(0)
    , _reconnectPending(false)
    , _reconnectDelay(RECONNECT_DELAY_MIN_MS)
    , _connectStartedAt(0)
    , _connectingNow(false)
    , _onConnectCb(nullptr)
    , _onDisconnectCb(nullptr)
{
    _instance = this;
}

// -----------------------------------------------------------------------------
// Public: begin
// -----------------------------------------------------------------------------

void WifiMgr::begin(const String& apPassword) {
    // Enable dual AP+STA mode
    WiFi.mode(WIFI_AP_STA);

    // Register WiFi event handler (non-blocking reconnect via events)
    WiFi.onEvent(_wifiEventHandler);

    // Build AP SSID and configure AP
    _setupAP(apPassword);
}

// -----------------------------------------------------------------------------
// Public: update — call from loop()
// -----------------------------------------------------------------------------

void WifiMgr::update() {
    unsigned long now = millis();

    // Check if an ongoing connection attempt has timed out
    if (_connectingNow) {
        if (WiFi.status() == WL_CONNECTED) {
            // Connected! Event handler will have set _staConnected = true.
            _connectingNow = false;
        } else if (now - _connectStartedAt >= CONNECT_TIMEOUT_MS) {
            // Timed out — try next network in priority list
            _connectingNow = false;
            _currentNetworkIndex++;
            _tryNextNetwork();
        }
    }

    // Handle scheduled reconnect
    if (_reconnectPending && !_connectingNow) {
        if (now >= _reconnectAt) {
            _reconnectPending = false;
            _currentNetworkIndex = 0;  // restart from highest priority
            _tryNextNetwork();
        }
    }
}

// -----------------------------------------------------------------------------
// Public: setNetworks
// -----------------------------------------------------------------------------

void WifiMgr::setNetworks(const std::vector<WifiCredentials>& networks) {
    _networkCount = 0;

    // Copy up to MAX_NETWORKS entries
    for (size_t i = 0; i < networks.size() && _networkCount < MAX_NETWORKS; i++) {
        _networks[_networkCount++] = networks[i];
    }

    // Sort by priority (lowest value = highest priority)
    std::sort(_networks, _networks + _networkCount,
        [](const WifiCredentials& a, const WifiCredentials& b) {
            return a.priority < b.priority;
        });
}

// -----------------------------------------------------------------------------
// Public: connectSTA
// -----------------------------------------------------------------------------

void WifiMgr::connectSTA() {
    _currentNetworkIndex = 0;
    _reconnectPending = false;
    _reconnectDelay = RECONNECT_DELAY_MIN_MS;
    _tryNextNetwork();
}

// -----------------------------------------------------------------------------
// Status queries
// -----------------------------------------------------------------------------

bool WifiMgr::isSTAConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WifiMgr::getSTAIP() const {
    return WiFi.localIP();
}

IPAddress WifiMgr::getAPIP() const {
    return WiFi.softAPIP();
}

int WifiMgr::getSignalStrength() const {
    if (!isSTAConnected()) return 0;
    return WiFi.RSSI();
}

String WifiMgr::getSSID() const {
    if (!isSTAConnected()) return "";
    return WiFi.SSID();
}

// -----------------------------------------------------------------------------
// Callback registration
// -----------------------------------------------------------------------------

void WifiMgr::registerOnConnect(WifiEventCallback cb) {
    _onConnectCb = cb;
}

void WifiMgr::registerOnDisconnect(WifiEventCallback cb) {
    _onDisconnectCb = cb;
}

// -----------------------------------------------------------------------------
// Private: _tryNextNetwork
// -----------------------------------------------------------------------------

void WifiMgr::_tryNextNetwork() {
    if (_networkCount == 0) return;

    if (_currentNetworkIndex >= _networkCount) {
        // Exhausted all networks — schedule reconnect with backoff
        _reconnectPending = true;
        _reconnectAt = millis() + _reconnectDelay;

        // Exponential backoff (double delay, cap at max)
        _reconnectDelay *= 2;
        if (_reconnectDelay > RECONNECT_DELAY_MAX_MS) {
            _reconnectDelay = RECONNECT_DELAY_MAX_MS;
        }
        return;
    }

    const WifiCredentials& cred = _networks[_currentNetworkIndex];
    WiFi.begin(cred.ssid.c_str(), cred.password.c_str());

    _connectStartedAt = millis();
    _connectingNow = true;
}

// -----------------------------------------------------------------------------
// Private: _setupAP
// -----------------------------------------------------------------------------

void WifiMgr::_setupAP(const String& password) {
    _apSsid = _buildAPSsid();

    // Configure AP IP address (portal: 10.0.0.1)
    IPAddress apIP(10, 0, 0, 1);
    IPAddress gateway(10, 0, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    // Start AP (open network if password is empty)
    if (password.length() > 0) {
        WiFi.softAP(_apSsid.c_str(), password.c_str(), AP_CHANNEL);
    } else {
        WiFi.softAP(_apSsid.c_str(), nullptr, AP_CHANNEL);
    }

    _apIP = WiFi.softAPIP();
}

String WifiMgr::_buildAPSsid() {
    // Get MAC address and use last 4 hex chars (last 2 bytes)
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);

    return String(AP_SSID_PREFIX) + "-" + String(suffix);
}

// -----------------------------------------------------------------------------
// Static: WiFi event handler
// -----------------------------------------------------------------------------

void WifiMgr::_wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance == nullptr) return;

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            // STA connected and got IP
            _instance->_staConnected = true;
            _instance->_connectingNow = false;
            _instance->_reconnectPending = false;
            _instance->_reconnectDelay = RECONNECT_DELAY_MIN_MS; // reset backoff
            if (_instance->_onConnectCb) {
                _instance->_onConnectCb();
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // STA lost connection — schedule non-blocking reconnect
            _instance->_staConnected = false;
            _instance->_connectingNow = false;
            _instance->_currentNetworkIndex = 0; // retry from highest priority
            _instance->_reconnectPending = true;
            _instance->_reconnectAt = millis() + _instance->_reconnectDelay;
            // Backoff for next failure
            _instance->_reconnectDelay *= 2;
            if (_instance->_reconnectDelay > RECONNECT_DELAY_MAX_MS) {
                _instance->_reconnectDelay = RECONNECT_DELAY_MAX_MS;
            }
            if (_instance->_onDisconnectCb) {
                _instance->_onDisconnectCb();
            }
            break;

        default:
            break;
    }
}

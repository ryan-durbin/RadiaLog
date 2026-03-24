#include "wifi_mgr.h"

// =============================================================================
// RadiaLog Firmware - WiFi Manager Implementation
// ESP32 WIFI_AP_STA dual mode:
//   AP:  RadiaLog-XXXX always on (XXXX = last 4 hex chars of MAC)
//   STA: connects to configured networks for upload
// =============================================================================

WifiMgr::WifiMgr()
    : _staConnected(false)
    , _apIP()
{
}

void WifiMgr::begin(const String& apPassword) {
    // Enable dual AP+STA mode
    WiFi.mode(WIFI_AP_STA);

    // Build AP SSID and configure AP
    _setupAP(apPassword);
}

void WifiMgr::_setupAP(const String& password) {
    _apSsid = _buildAPSsid();

    // Configure AP IP address
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

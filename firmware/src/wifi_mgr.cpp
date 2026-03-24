#include "wifi_mgr.h"

// =============================================================================
// RadiaLog Firmware - WifiMgr Stub Implementation
// Real implementation uses WiFi.h for AP+STA management.
// =============================================================================

WifiMgr::WifiMgr()
    : _staConnected(false)
    , _staRSSI(0)
    , _apClients(0)
{
}

void WifiMgr::begin(ConfigMgr& cfg) {
    // Stub: no WiFi initialization
    // Real implementation: WiFi.softAP(...), WiFi.begin(ssid, pass)
    (void)cfg;
}

bool WifiMgr::isStaConnected() const {
    return _staConnected;
}

int WifiMgr::getStaRSSI() const {
    return _staRSSI;
}

int WifiMgr::getApClients() const {
    return _apClients;
}

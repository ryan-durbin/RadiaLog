#include "portal.h"

// =============================================================================
// RadiaLog Firmware - StatusPortal Stub Implementation
// Real implementation registers ESPAsyncWebServer routes for status/config/logs.
// =============================================================================

StatusPortal::StatusPortal()
    : _cfg(nullptr)
    , _buf(nullptr)
    , _rc(nullptr)
{
}

void StatusPortal::begin(ConfigMgr& cfg, ReadingBuffer& buf, radiacode::RadiaCode& rc) {
    _cfg = &cfg;
    _buf = &buf;
    _rc  = &rc;
    // Stub: no server initialization
    // Real implementation:
    //   AsyncWebServer server(80);
    //   server.on("/", ...) → serve status JSON
    //   server.begin();
}

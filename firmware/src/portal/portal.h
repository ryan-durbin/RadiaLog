#pragma once
#ifndef PORTAL_H
#define PORTAL_H

#include <Arduino.h>
#include "../config_mgr.h"
#include "../buffer.h"
#include "../radiacode.h"

// =============================================================================
// RadiaLog Firmware - Status Portal
// Async web server providing device status, log streaming, and config UI.
// =============================================================================

class StatusPortal {
public:
    StatusPortal();

    /// Start the async web server and register all routes.
    void begin(ConfigMgr& cfg, ReadingBuffer& buf, radiacode::RadiaCode& rc);

private:
    ConfigMgr*           _cfg;
    ReadingBuffer*        _buf;
    radiacode::RadiaCode* _rc;
};

#endif // PORTAL_H

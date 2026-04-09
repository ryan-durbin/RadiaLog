#pragma once
#ifndef PORTAL_H
#define PORTAL_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../config_mgr.h"
#include "../buffer.h"
#include "../radiacode.h"
#include "../radiacode_mgr.h"
#include "../gps/gps.h"
#include "../wifi_mgr.h"
#include "../uploader.h"

// =============================================================================
// Shutdown flag — set by POST /api/actions/shutdown, checked in main loop
// =============================================================================
extern volatile bool g_shutdownRequested;

// =============================================================================
// RadiaLog Firmware - Status Portal
// Async web server providing device status, log streaming, config UI,
// data export, and OTA firmware updates. Serves on port 80.
// =============================================================================

class StatusPortal {
public:
    StatusPortal();

    /// Start the async web server and register all routes.
    void begin(ConfigMgr& cfg, ReadingBuffer& buf, radiacode::RadiaCode& rc,
               GPS& gps, WifiMgr& wifi, Uploader& uploader,
               radiacode::RadiaCodeMgr* rcMgr = nullptr);

    /// Update latest radiation reading values (called from main loop).
    void updateReading(float doseRate, float countRate);

    /// Update battery voltage (called from main loop).
    void updateBattery(float voltage, uint8_t percent);

    /// Update time sync status with source ("NTP" or "GPS").
    void setTimeSyncSource(const String& source);

    /// Update performance metrics (called from main loop).
    void updatePerf(float loopAvgUs, unsigned long loopMaxUs, float cpuPct,
                    uint32_t freeHeap, uint32_t minFreeHeap, uint8_t cpuMHz);

private:
    AsyncWebServer*       _server;
    ConfigMgr*            _cfg;
    ReadingBuffer*        _buf;
    radiacode::RadiaCode* _rc;
    GPS*                  _gps;
    WifiMgr*              _wifi;
    Uploader*             _uploader;
    radiacode::RadiaCodeMgr* _rcMgr;

    volatile float   _doseRate;
    volatile float   _countRate;
    volatile float   _batteryVoltage;
    volatile uint8_t _batteryPercent;
    String           _timeSyncSource;  // "" = not synced, "NTP", "GPS"

    // Performance metrics
    volatile float         _perfLoopAvgUs;
    volatile unsigned long _perfLoopMaxUs;
    volatile float         _perfCpuPct;
    volatile uint32_t      _perfFreeHeap;
    volatile uint32_t      _perfMinFreeHeap;
    volatile uint8_t       _perfCpuMHz;

    // Route handlers
    void _registerRoutes();
    void _handleApiStatus(AsyncWebServerRequest* request);
    void _handleApiSettings(AsyncWebServerRequest* request);
    void _handleApiReadings(AsyncWebServerRequest* request);
    void _handleApiReadingsCsv(AsyncWebServerRequest* request);
};

#endif // PORTAL_H

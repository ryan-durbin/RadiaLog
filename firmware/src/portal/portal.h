#pragma once
#ifndef PORTAL_H
#define PORTAL_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../config_mgr.h"
#include "../buffer.h"
#include "../radiacode.h"
#include "../gps/gps.h"
#include "../wifi_mgr.h"
#include "../uploader.h"

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
               GPS& gps, WifiMgr& wifi, Uploader& uploader);

    /// Update latest radiation reading values (called from main loop).
    void updateReading(float doseRate, float countRate);

    /// Update battery voltage (called from main loop).
    void updateBattery(float voltage, uint8_t percent);

    /// Update NTP sync status.
    void setTimeSynced(bool synced);

private:
    AsyncWebServer*       _server;
    ConfigMgr*            _cfg;
    ReadingBuffer*        _buf;
    radiacode::RadiaCode* _rc;
    GPS*                  _gps;
    WifiMgr*              _wifi;
    Uploader*             _uploader;

    volatile float   _doseRate;
    volatile float   _countRate;
    volatile float   _batteryVoltage;
    volatile uint8_t _batteryPercent;
    volatile bool    _timeSynced;

    // Route handlers
    void _registerRoutes();
    void _handleApiStatus(AsyncWebServerRequest* request);
    void _handleApiSettings(AsyncWebServerRequest* request);
    void _handleApiReadings(AsyncWebServerRequest* request);
    void _handleApiReadingsCsv(AsyncWebServerRequest* request);
};

#endif // PORTAL_H

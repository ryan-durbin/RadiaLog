#include <Arduino.h>
#include "config.h"
#include "reading.h"
#include "config_mgr.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include "led.h"
#include "uploader.h"
#include "radiacode.h"
#include "gps/atgm336h.h"
#include "portal/portal.h"
#include "portal/debug_ws.h"

// =============================================================================
// RadiaLog Firmware - Main Entry Point
// Board: Seeed XIAO ESP32S3
// =============================================================================
// Phase 1-3 integration: wires all modules together.
//   - radiacode.h/cpp  (USB Host protocol)
//   - gps/atgm336h.h/cpp (GPS UART driver)
//   - buffer.h/cpp (LittleFS reading storage)
//   - config_mgr.h/cpp (JSON config from LittleFS)
//   - wifi_mgr.h/cpp (AP + STA WiFi)
//   - led.h/cpp (Status LED patterns)
//   - uploader.h/cpp (FreeRTOS HTTP batch upload on core 0)
//   - portal/portal.h/cpp (Async web status portal)
//   - portal/debug_ws.h/cpp (WebSocket debug log streaming)
// =============================================================================

// --- Module instances --------------------------------------------------------
ConfigMgr               configMgr;
ReadingBuffer           readingBuffer;
static WifiMgr          wifi;
static Led              led;
static Uploader         uploader;
static radiacode::RadiaCode radiaCode;
static ATGM336H         gps(Serial1, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
static StatusPortal     portal;

// =============================================================================
// setup()
// =============================================================================
void setup() {
    // 1. Serial debug output (FTDI) — must be first
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    Serial.println(F("[RadiaLog] Firmware starting..."));

    // 2. Load configuration from LittleFS (/config.json)
    //    ConfigMgr handles LittleFS.begin() internally
    configMgr.load();

    // 3. Initialize debug WebSocket logger
    //    All subsequent log output goes to both Serial and portal clients
    debugWS.begin(nullptr);   // server registered later in portal.begin()
    debugWS.log(BUFFER, INFO, "[RadiaLog] Config loaded. Upload URL: " + configMgr.getUploadUrl());

    // 4. Initialize LittleFS reading buffer
    readingBuffer.begin();
    {
        BufferStats stats = readingBuffer.getStats();
        debugWS.log(BUFFER, INFO, "[RadiaLog] Buffer ready. Stored: " + String(stats.depth)
            + " total, unsent: " + String(stats.depth - stats.lifetimeUploaded));
    }

    // 5. Initialize WiFi (AP + STA)
    wifi.begin(configMgr);
    debugWS.log(WIFI, INFO, "[RadiaLog] WiFi initialized.");

    // 6. Initialize status portal (async web server)
    portal.begin(configMgr, readingBuffer, radiaCode);
    debugWS.log(BUFFER, INFO, "[RadiaLog] Status portal started.");

    // 7. Initialize upload manager (FreeRTOS task on core 0)
    uploader.begin(configMgr, readingBuffer);
    debugWS.log(UPLOAD, INFO, "[RadiaLog] Uploader started.");

    // 8. Initialize RadiaCode USB Host
    {
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            debugWS.log(USB, INFO, "[RadiaLog] RadiaCode USB connected.");
        } else {
            debugWS.log(USB, WARN, "[RadiaLog] WARNING: RadiaCode USB not found (err="
                + String(static_cast<int>(err)) + "). Will retry in loop.");
        }
    }

    // 9. Initialize GPS UART
    gps.begin();
    debugWS.log(GPS, INFO, "[RadiaLog] GPS initialized. TX=" + String(GPS_TX_PIN)
        + " RX=" + String(GPS_RX_PIN) + " BAUD=" + String(GPS_BAUD));

    // 10. Set initial LED pattern
    led.begin();
    if (!radiaCode.isConnected()) {
        led.setPattern(LedPattern::DOUBLE_FLASH);   // error: no USB
    } else {
        led.setPattern(LedPattern::SLOW_BLINK);     // running, waiting for GPS fix
    }

    debugWS.log(USB, INFO, "[RadiaLog] Setup complete. Entering loop.");
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    // --- 1. Poll RadiaCode USB --------------------------------------------------
    bool usbOk = false;
    float dose_rate  = 0.0f;
    float count_rate = 0.0f;

    if (radiaCode.isConnected()) {
        std::vector<uint8_t> response;
        auto req = radiaCode.buildRequest(0x0005);
        radiacode::Error err = radiaCode.execute(req, response);
        if (err == radiacode::Error::OK) {
            usbOk = true;
        } else {
            debugWS.log(USB, WARN, "[RadiaLog] WARNING: RadiaCode USB read error (err="
                + String(static_cast<int>(err)) + "). Retrying.");
            if (!radiaCode.isConnected()) {
                debugWS.log(USB, WARN, "[RadiaLog] WARNING: RadiaCode USB disconnected.");
                led.setPattern(LedPattern::DOUBLE_FLASH);
            }
        }
    } else {
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            debugWS.log(USB, INFO, "[RadiaLog] RadiaCode USB reconnected.");
            usbOk = true;
        }
    }

    // --- 2. Poll GPS -----------------------------------------------------------
    bool gpsPolled = gps.poll();
    bool gpsFix    = gps.hasFix();
    (void)gpsPolled;

    // --- 3. Merge and store reading --------------------------------------------
    if (usbOk) {
        Reading r;
        r.timestamp         = millis() / 1000UL;
        r.dose_rate         = dose_rate;
        r.count_rate        = count_rate;
        r.gps_valid         = gpsFix;
        r.lat               = gpsFix ? gps.getLat()      : 0.0;
        r.lon               = gpsFix ? gps.getLon()      : 0.0;
        r.altitude          = gpsFix ? gps.getAlt()      : 0.0f;
        r.speed_mph         = gpsFix ? (gps.getSpeed() * 0.621371f) : 0.0f;
        r.speed_kph         = gpsFix ? gps.getSpeed()    : 0.0f;
        r.heading           = gpsFix ? gps.getHeading()  : 0.0f;
        r.accuracy          = gpsFix ? gps.getAccuracy() : 0.0f;
        r.altitude_accuracy = 0.0f;

        readingBuffer.appendReading(r);
    }

    // --- 4. Update LED pattern based on current state -------------------------
    if (!radiaCode.isConnected()) {
        led.setPattern(LedPattern::DOUBLE_FLASH);
    } else if (uploader.isUploading()) {
        led.setPattern(LedPattern::FAST_BLINK);
    } else if (!gpsFix) {
        led.setPattern(LedPattern::SLOW_BLINK);
    } else {
        led.setPattern(LedPattern::SOLID);
    }
    led.update();

    // --- 5. Upload manager runs independently (core 0 FreeRTOS task) ----------
    // --- 6. Portal runs independently (AsyncWebServer callbacks) ---------------

    // --- 7. Wait until next reading interval ----------------------------------
    delay(configMgr.getReadingIntervalMs());
}

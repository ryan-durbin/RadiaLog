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
static ConfigMgr          config;
static ReadingBuffer      buffer;
static WifiMgr            wifi;
static Led                led;
static Uploader           uploader;
static radiacode::RadiaCode radiaCode;
static ATGM336H           gps(Serial1, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
static StatusPortal       portal;

// =============================================================================
// setup()
// =============================================================================
void setup() {
    // 1. Serial debug output (FTDI)
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    Serial.println(F("[RadiaLog] Firmware starting..."));

    // 2. Load configuration from LittleFS (/config.json)
    config.load();
    Serial.printf("[RadiaLog] Config loaded. Upload URL: %s\n",
                  config.getUploadUrl().c_str());

    // 3. Initialize debug WebSocket logger
    //    (so all subsequent log output goes to both Serial and portal clients)
    debugWS.begin(nullptr);   // server registered later in portal.begin()

    // 4. Initialize LittleFS reading buffer
    buffer.begin();
    Serial.printf("[RadiaLog] Buffer ready. Stored readings: %u\n", buffer.count());

    // 5. Initialize WiFi (AP + STA)
    wifi.begin(config);
    Serial.println(F("[RadiaLog] WiFi initialized."));

    // 6. Initialize status portal (async web server)
    portal.begin(config, buffer, radiaCode);
    Serial.println(F("[RadiaLog] Status portal started."));

    // 7. Initialize upload manager (FreeRTOS task on core 0)
    uploader.begin(config, buffer);
    Serial.println(F("[RadiaLog] Uploader started."));

    // 8. Initialize RadiaCode USB Host
    {
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            Serial.println(F("[RadiaLog] RadiaCode USB connected."));
        } else {
            Serial.printf("[RadiaLog] WARNING: RadiaCode USB not found (err=%d). Will retry in loop.\n",
                          static_cast<int>(err));
        }
    }

    // 9. Initialize GPS UART
    gps.begin();
    Serial.printf("[RadiaLog] GPS initialized. TX=%d RX=%d BAUD=%u\n",
                  GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);

    // 10. Set initial LED pattern
    led.begin();
    if (!radiaCode.isConnected()) {
        led.setPattern(LedPattern::DOUBLE_FLASH);   // error: no USB
    } else {
        led.setPattern(LedPattern::SLOW_BLINK);     // running, waiting for GPS fix
    }

    Serial.println(F("[RadiaLog] Setup complete. Entering loop."));
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
        // Build and execute a data-request command
        // (real implementation sends the "get spectrum / count" command)
        std::vector<uint8_t> response;
        auto req = radiaCode.buildRequest(0x0005);  // placeholder command ID
        radiacode::Error err = radiaCode.execute(req, response);
        if (err == radiacode::Error::OK) {
            // TODO: parse dose_rate and count_rate from response
            usbOk = true;
        } else {
            Serial.printf("[RadiaLog] WARNING: RadiaCode USB read error (err=%d). Retrying.\n",
                          static_cast<int>(err));
            // If device disconnected, log and keep running
            if (!radiaCode.isConnected()) {
                Serial.println(F("[RadiaLog] WARNING: RadiaCode USB disconnected."));
                led.setPattern(LedPattern::DOUBLE_FLASH);
            }
        }
    } else {
        // Retry connection
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            Serial.println(F("[RadiaLog] RadiaCode USB reconnected."));
            usbOk = true;
        }
    }

    // --- 2. Poll GPS -----------------------------------------------------------
    bool gpsPolled = gps.poll();
    bool gpsFix    = gps.hasFix();

    // --- 3. Merge and store reading --------------------------------------------
    //    Store reading regardless of GPS fix. If no fix: lat=0 lon=0 gps_valid=false
    if (usbOk) {
        Reading r;
        r.timestamp         = millis() / 1000UL;  // approximate; real: NTP or GPS time
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

        buffer.append(r);
    }

    // --- 4. Update LED pattern based on current state -------------------------
    if (!radiaCode.isConnected()) {
        led.setPattern(LedPattern::DOUBLE_FLASH);   // USB error
    } else if (uploader.isUploading()) {
        led.setPattern(LedPattern::FAST_BLINK);     // uploading
    } else if (!gpsFix) {
        led.setPattern(LedPattern::SLOW_BLINK);     // no GPS fix
    } else {
        led.setPattern(LedPattern::SOLID);          // all good
    }
    led.update();

    // --- 5. Upload manager runs independently (core 0 FreeRTOS task) ----------
    // --- 6. Portal runs independently (AsyncWebServer callbacks) ---------------

    // --- 7. Wait until next reading interval ----------------------------------
    delay(config.getReadingIntervalMs());
}

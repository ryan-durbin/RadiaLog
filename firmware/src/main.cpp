#include <Arduino.h>
#include <time.h>
#include <ESPmDNS.h>
#include "config.h"
#include "config_mgr.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include "led.h"
#include "uploader.h"
#include "radiacode.h"
#include "battery.h"
#include "gps/atgm336h.h"
#include "portal/portal.h"
#include "portal/debug_ws.h"

#ifdef HAS_DISPLAY
#include "display.h"
#endif

// =============================================================================
// RadiaLog Firmware - Main Entry Point
// Board: Seeed XIAO ESP32S3 or LilyGO T-Display S3
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
static Battery          battery;

#ifdef HAS_DISPLAY
static Display          display;
#endif

// --- NTP state ---------------------------------------------------------------
static volatile bool ntpSynced = false;

static bool isTimeSynced() {
    return time(nullptr) > 1000000000;
}

static uint32_t getCurrentTimestamp() {
    time_t now = time(nullptr);
    if (now > 1000000000) return static_cast<uint32_t>(now);
    return millis() / 1000UL;
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
    // 1. Serial debug output
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    Serial.println(F("[RadiaLog] Firmware v" FW_VERSION " starting..."));

    // 2. Load configuration from LittleFS
    configMgr.load();

    // 3. Initialize debug WebSocket logger
    debugWS.begin(nullptr);
    debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] Config loaded. Upload URL: " + configMgr.getUploadUrl());

    // 4. Initialize LittleFS reading buffer
    readingBuffer.begin();
    {
        BufferStats stats = readingBuffer.getStats();
        debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] Buffer ready. Stored: " + String(stats.depth)
            + ", pending: " + String(stats.pending));
    }

    // 5. Initialize WiFi (AP + STA)
    wifi.begin(configMgr.getApPassword());
    {
        std::vector<WifiCredentials> networks;
        for (int i = 0; i < configMgr.getWifiCount(); i++) {
            WifiCredentials cred;
            cred.ssid     = configMgr.getWifiSSID(i);
            cred.password = configMgr.getWifiPass(i);
            cred.priority = static_cast<uint8_t>(i);
            networks.push_back(cred);
        }
        if (!networks.empty()) {
            wifi.setNetworks(networks);
            wifi.connectSTA();
        }
    }
    debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] WiFi initialized. AP: " + wifi.getAPIP().toString());

    // 6. NTP sync + mDNS on WiFi connect
    wifi.registerOnConnect([]() {
        configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
        if (MDNS.begin("radialog")) {
            MDNS.addService("http", "tcp", 80);
            debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] mDNS started: http://radialog.local");
        }
        debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] STA IP: " + WiFi.localIP().toString());
    });

    // 7. Status portal
    portal.begin(configMgr, readingBuffer, radiaCode, gps, wifi, uploader);
    debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] Status portal started on " + wifi.getAPIP().toString());

    // 8. Upload manager (FreeRTOS task on core 0)
    {
        UploadConfig uploadCfg;
        uploadCfg.radiamaps_url = configMgr.getUploadUrl();
        uploadCfg.device_token  = configMgr.getToken();
        uploadCfg.device_id     = configMgr.getDeviceId();
        uploader.begin(uploadCfg, &readingBuffer, &wifi);
    }
    debugWS.log(MOD_UPLOAD, LVL_INFO, "[RadiaLog] Uploader started.");

    // 9. RadiaCode USB Host
    {
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            debugWS.log(MOD_USB, LVL_INFO, "[RadiaLog] RadiaCode USB connected.");
            bool initOk = radiaCode.init();
            if (initOk) {
                debugWS.log(MOD_USB, LVL_INFO, "[RadiaLog] RadiaCode init sequence complete.");
            } else {
                debugWS.log(MOD_USB, LVL_WARN, "[RadiaLog] WARNING: RadiaCode init() failed.");
            }
        } else {
            debugWS.log(MOD_USB, LVL_WARN, "[RadiaLog] WARNING: RadiaCode USB not found (err="
                + String(static_cast<int>(err)) + "). Will retry in loop.");
        }
    }

    // 10. GPS UART
    gps.begin();
    debugWS.log(MOD_GPS, LVL_INFO, "[RadiaLog] GPS initialized. TX=" + String(GPS_TX_PIN)
        + " RX=" + String(GPS_RX_PIN));

    // 11. Battery ADC
    battery.begin();

    // 12. LED
    led.begin();
    if (!radiaCode.isConnected()) {
        led.setPattern(LedPattern::DOUBLE_FLASH);
    } else {
        led.setPattern(LedPattern::SLOW_BLINK);
    }

    // 13. TFT Display (T-Display S3 only)
#ifdef HAS_DISPLAY
    display.begin();
    debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] TFT display initialized.");
#endif

    debugWS.log(MOD_USB, LVL_INFO, "[RadiaLog] Setup complete. Entering loop.");
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    // --- 0. WiFi reconnect ---------------------------------------------------
    wifi.update();

    // --- 1. Poll RadiaCode USB -----------------------------------------------
    bool usbOk = false;
    float dose_rate  = 0.0f;
    float count_rate = 0.0f;

    if (radiaCode.isConnected()) {
        auto readings = radiaCode.readDataBuf();
        if (!readings.empty()) {
            auto& latest = readings.back();
            dose_rate  = latest.dose_rate;
            count_rate = latest.count_rate;
            usbOk = true;
        } else {
            usbOk = radiaCode.isConnected();
        }
    } else {
        radiacode::Error err = radiaCode.connect();
        if (err == radiacode::Error::OK) {
            debugWS.log(MOD_USB, LVL_INFO, "[RadiaLog] RadiaCode USB reconnected.");
            usbOk = true;
        }
    }

    // --- 2. Poll GPS ---------------------------------------------------------
    gps.poll();
    bool gpsFix = gps.hasFix();

    // --- 3. Store reading ----------------------------------------------------
    if (usbOk) {
        Reading r = {};
        r.timestamp         = getCurrentTimestamp();
        r.dose_rate         = dose_rate;
        r.count_rate        = count_rate;
        r.gps_valid         = gpsFix;
        r.lat               = gpsFix ? static_cast<float>(gps.getLat()) : 0.0f;
        r.lon               = gpsFix ? static_cast<float>(gps.getLon()) : 0.0f;
        r.altitude          = gpsFix ? gps.getAlt()      : 0.0f;
        r.speed_mph         = gpsFix ? (gps.getSpeed() * 0.621371f) : 0.0f;
        r.speed_kph         = gpsFix ? gps.getSpeed()    : 0.0f;
        r.heading           = gpsFix ? gps.getHeading()  : 0.0f;
        r.accuracy          = gpsFix ? gps.getAccuracy() : 0.0f;
        r.altitude_accuracy = 0.0f;

        readingBuffer.appendReading(r);
    }

    // --- 4. Battery ----------------------------------------------------------
    battery.update();

    // --- 5. Update portal live data ------------------------------------------
    portal.updateReading(dose_rate, count_rate);
    portal.updateBattery(battery.getVoltage(), battery.getPercent());

    if (!ntpSynced && isTimeSynced()) {
        ntpSynced = true;
        portal.setTimeSynced(true);
        debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] NTP time synced.");
    }

    // --- 6. LED pattern ------------------------------------------------------
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

    // --- 7. TFT Display (T-Display S3 only) ----------------------------------
#ifdef HAS_DISPLAY
    display.handleButton();
    if (display.isOn()) {
        BufferStats stats = readingBuffer.getStats();

        // Compute last upload string
        String lastUpStr = "Never";
        unsigned long lastUp = uploader.getLastUploadTime();
        if (lastUp > 0) {
            unsigned long ago = (millis() - lastUp) / 1000;
            if (ago < 60) lastUpStr = String(ago) + "s ago";
            else if (ago < 3600) lastUpStr = String(ago / 60) + "m ago";
            else lastUpStr = String(ago / 3600) + "h ago";
        }

        DisplayStatus ds;
        ds.usbConnected   = radiaCode.isConnected();
        ds.wifiConnected  = wifi.isSTAConnected();
        ds.wifiSSID       = wifi.getSSID();
        ds.staIP          = wifi.getSTAIP().toString();
        ds.lastUpload     = lastUpStr;
        ds.totalReadings  = stats.depth;
        ds.pendingReadings = stats.pending;
        ds.doseRate       = dose_rate;
        ds.countRate      = count_rate;
        ds.batteryVoltage = battery.getVoltage();
        ds.batteryPercent = battery.getPercent();
        ds.gpsFix         = gpsFix;
        ds.gpsSats        = gps.getSatellites();

        display.draw(ds);
    }
#endif

    // --- 8. Buffer maintenance -----------------------------------------------
    readingBuffer.pruneUploaded();

    // --- 9. Wait until next reading interval ---------------------------------
    delay(configMgr.getReadingIntervalMs());
}

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <esp_pm.h>
#include <ESPmDNS.h>
#include "config.h"
#include "shipping_mode.h"
#include "config_mgr.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include "led.h"
#include "uploader.h"
#include "radiacode.h"
#include "radiacode_mgr.h"
#include "gps/gps.h"
#ifdef GPS_TYPE_I2C
#include <Wire.h>
#include "gps/lc76g_i2c.h"
#else
#include "gps/atgm336h.h"
#endif

#ifdef BATTERY_TYPE_AXP2101
#include "battery_axp2101.h"
#else
#include "battery.h"
#endif
#include "location_provider.h"
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
static radiacode::RadiaCodeMgr radiaCodeMgr;
#ifdef GPS_TYPE_I2C
static LC76G_I2C        gps(Wire, GPS_I2C_SDA, GPS_I2C_SCL, GPS_I2C_ADDR);
#else
static ATGM336H        gps(Serial1, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
#endif
static StatusPortal     portal;
static ShippingMode     shippingMode;
#ifdef BATTERY_TYPE_AXP2101
static BatteryAXP2101   battery;
#else
static Battery          battery;
#endif
static LocationProvider locationProvider;

#ifdef HAS_DISPLAY
static Display          display;
#endif

// --- Time sync state ---------------------------------------------------------
static volatile bool timeSynced = false;
static volatile bool ntpSyncedFlag = false;  // Set by SNTP callback when NTP actually syncs
static String timeSyncSource = "";  // "NTP" or "GPS"

// --- Performance metrics -----------------------------------------------------
struct PerfStats {
    unsigned long loopDurationUs;     // last loop duration (microseconds)
    unsigned long loopMaxUs;          // max loop duration seen
    float         loopAvgUs;          // exponential moving average
    float         cpuUsagePct;        // estimated CPU usage (loop work / total interval)
    uint32_t      freeHeap;           // free heap bytes
    uint32_t      minFreeHeap;        // minimum free heap since boot
    uint8_t       cpuFreqMHz;         // current CPU frequency
};
static PerfStats perfStats = {};

static void onNtpSync(struct timeval* tv) {
    ntpSyncedFlag = true;
}

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

    // Power management: drop CPU to 80MHz, enable light sleep during idle
    setCpuFrequencyMhz(80);
#ifdef CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {};
    pm_config.max_freq_mhz = 80;
    pm_config.min_freq_mhz = 10;
    pm_config.light_sleep_enable = true;
    esp_err_t pm_err = esp_pm_configure(&pm_config);
    if (pm_err == ESP_OK) {
        Serial.println(F("[RadiaLog] PM configured: 80MHz max, 10MHz min, light sleep enabled"));
    } else {
        Serial.printf("[RadiaLog] PM config failed (%d), using static 80MHz\n", pm_err);
    }
#else
    Serial.println(F("[RadiaLog] CPU frequency: 80 MHz (PM not available)"));
#endif

    // Enable Vext power rail (Heltec Wireless Tracker: powers GPS + display)
#ifdef VEXT_PIN
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, VEXT_ACTIVE_HIGH ? HIGH : LOW);
    delay(20);  // Allow peripherals to power up
#endif

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

        // Seed lifetime counter from buffer if NVS counter is behind
        // (e.g. counter added after device already had readings)
        if (configMgr.getTotalReadingsLogged() < stats.lifetimeLogged) {
            configMgr.setTotalReadingsLogged(stats.lifetimeLogged);
            configMgr.flushTotalReadingsLogged();
            debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] Seeded lifetime counter from buffer: " + String(stats.lifetimeLogged));
        }
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

    // 6. NTP sync + mDNS + upload check on WiFi connect
    sntp_set_time_sync_notification_cb(onNtpSync);
    wifi.registerOnConnect([]() {
        configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
        if (MDNS.begin("radialog")) {
            MDNS.addService("http", "tcp", 80);
            debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] mDNS started: http://radialog.local");
        }
        debugWS.log(MOD_WIFI, LVL_INFO, "[RadiaLog] STA IP: " + WiFi.localIP().toString());
        uploader.onWifiConnect();
    });

    // 7. Status portal
    portal.begin(configMgr, readingBuffer, radiaCode, gps, wifi, uploader, &radiaCodeMgr);
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

    // 9b. RadiaCode device manager (USB + BLE)
    {
        std::vector<String> bleMacs;
        for (int i = 0; i < configMgr.getBleDeviceCount(); i++) {
            bleMacs.push_back(configMgr.getBleDeviceMac(i));
        }
        radiaCodeMgr.begin(radiaCode, bleMacs);
        debugWS.log(MOD_USB, LVL_INFO,
            "[RadiaLog] Device manager ready. BLE devices: " + String(bleMacs.size()));
    }

    // 10. GPS
    gps.begin();
#ifdef GPS_TYPE_I2C
    debugWS.log(MOD_GPS, LVL_INFO, "[RadiaLog] GPS initialized (I2C). SDA=" + String(GPS_I2C_SDA)
        + " SCL=" + String(GPS_I2C_SCL));
#else
    debugWS.log(MOD_GPS, LVL_INFO, "[RadiaLog] GPS initialized (UART). TX=" + String(GPS_TX_PIN)
        + " RX=" + String(GPS_RX_PIN));
#endif

    // 10b. Location provider (clears cached position on boot)
    locationProvider.begin(configMgr.getGoogleApiKey());
    debugWS.log(MOD_GPS, LVL_INFO, "[RadiaLog] LocationProvider initialized.");

    // 11. Battery
#ifdef BATTERY_TYPE_AXP2101
    battery.begin(Wire, PMU_I2C_SDA, PMU_I2C_SCL);
#else
    battery.begin();
#endif

    // 12. LED
    led.begin();
    shippingMode.begin();
    if (radiaCodeMgr.connectedCount() == 0) {
        led.setPattern(LedPattern::DOUBLE_FLASH);
    } else {
        led.setPattern(LedPattern::SLOW_BLINK);
    }

    // 13. TFT Display (T-Display S3 only)
#ifdef HAS_DISPLAY
    display.begin();
    display.setTimeoutSec(configMgr.getDisplayTimeoutSec());
    display.setButtonWakeEnabled(configMgr.getButtonWakeEnabled());
    debugWS.log(MOD_BUFFER, LVL_INFO, "[RadiaLog] TFT display initialized. Timeout: "
        + String(configMgr.getDisplayTimeoutSec()) + "s, Button wake: "
        + String(configMgr.getButtonWakeEnabled() ? "on" : "off"));
#endif

    debugWS.log(MOD_USB, LVL_INFO, "[RadiaLog] Setup complete. Entering loop.");
}

// =============================================================================
// loop()
// =============================================================================
static uint32_t loopCounter = 0;

void loop() {
    unsigned long loopStartUs = micros();
    loopCounter++;

    // --- -1. Shipping mode check ---------------------------------------------
    shippingMode.update();
    if (shippingMode.shouldEnterSleep() || g_shutdownRequested) {
        g_shutdownRequested = false;  // clear flag (in case wake path re-runs)
        // Set TRIPLE_FLASH pattern and wait for completion
        led.setPattern(LedPattern::TRIPLE_FLASH);
        // Keep updating LED until pattern is complete
        while (!led.isPatternComplete()) {
            led.update();
            delay(10);
        }
        // Flush persistent state
        configMgr.flushTotalReadingsLogged();
        // Cut power to the GPS so it does not continue to drain the battery
        // while the ESP32 is in deep sleep.  No-op on boards without a
        // GPS_POWER_PIN wired up.
        gps.shutdown();
        Serial.println(F("[RadiaLog] Entering shipping mode (deep sleep)..."));
        Serial.flush();
        // No wakeup sources configured - only reset button wakes
        esp_deep_sleep_start();
    }

    // --- 0. WiFi reconnect ---------------------------------------------------
    wifi.update();

    // --- 1. Poll all RadiaCode devices (USB + BLE) ----------------------------
    auto deviceReadings = radiaCodeMgr.poll();
    bool anyDeviceOk = !deviceReadings.empty();

    // USB takes priority; fall back to BLE if no USB reading.
    // Static so we retain the last known value between polls.
    static float dose_rate  = 0.0f;
    static float count_rate = 0.0f;
    if (!deviceReadings.empty()) {
        bool foundUsb = false;
        for (const auto& dr : deviceReadings) {
            if (dr.deviceId == "USB") {
                dose_rate  = dr.dose_rate;
                count_rate = dr.count_rate;
                foundUsb = true;
                break;
            }
        }
        if (!foundUsb) {
            dose_rate  = deviceReadings[0].dose_rate;
            count_rate = deviceReadings[0].count_rate;
        }
    }

    // --- 2. Poll GPS + resolve location --------------------------------------
    gps.poll();
    bool locationValid = locationProvider.update(gps, wifi.isSTAConnected());

    // --- 3. Store readings (one per device, only with valid location) ---------
    if (locationValid) {
        for (const auto& dr : deviceReadings) {
            Reading r = {};
            r.timestamp         = getCurrentTimestamp();
            r.dose_rate         = dr.dose_rate;
            r.count_rate        = dr.count_rate;
            r.gps_valid         = true;
            r.lat               = static_cast<float>(locationProvider.getLat());
            r.lon               = static_cast<float>(locationProvider.getLon());
            r.altitude          = locationProvider.getAlt();
            r.speed_mph         = locationProvider.getSpeed() * 0.621371f;
            r.speed_kph         = locationProvider.getSpeed();
            r.heading           = locationProvider.getHeading();
            r.accuracy          = locationProvider.getAccuracy();
            r.altitude_accuracy = 0.0f;

            readingBuffer.appendReading(r);
            configMgr.incrementTotalReadingsLogged();
        }
    }

    // --- 4. Battery ----------------------------------------------------------
    bool wasCharging = battery.isCharging();
    battery.update();

    // Wake display when USB power is connected (charging state transitions off→on)
#ifdef HAS_DISPLAY
    if (!wasCharging && battery.isCharging()) {
        display.wake();
    }
#endif

    // --- 5. Update portal live data ------------------------------------------
    portal.updateReading(dose_rate, count_rate);
    portal.updateBattery(battery.getVoltage(), battery.getPercent());

    // --- Time sync: NTP (preferred) or GPS fallback ---
    if (ntpSyncedFlag) {
        // SNTP callback fired — NTP is authoritative regardless of prior source
        ntpSyncedFlag = false;
        if (timeSyncSource != "NTP") {
            timeSynced = true;
            bool upgraded = timeSyncSource == "GPS";
            timeSyncSource = "NTP";
            portal.setTimeSyncSource("NTP");
            debugWS.log(MOD_WIFI, LVL_INFO,
                upgraded ? "[RadiaLog] Time source upgraded from GPS to NTP."
                         : "[RadiaLog] NTP time synced.");
        }
    } else if (!timeSynced && gps.hasValidTime()) {
        // No NTP yet — set system clock from GPS UTC
        struct tm t = {};
        t.tm_year = gps.getYear() - 1900;
        t.tm_mon  = gps.getMonth() - 1;
        t.tm_mday = gps.getDay();
        t.tm_hour = gps.getHour();
        t.tm_min  = gps.getMinute();
        t.tm_sec  = gps.getSecond();
        time_t epoch = mktime(&t);  // ESP32 TZ defaults to UTC
        if (epoch > 1000000000) {
            struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
            settimeofday(&tv, nullptr);
            timeSynced = true;
            timeSyncSource = "GPS";
            portal.setTimeSyncSource("GPS");
            debugWS.log(MOD_GPS, LVL_INFO, "[RadiaLog] System time set from GPS.");
        }
    }

    // --- 6. LED pattern ------------------------------------------------------
    if (radiaCodeMgr.connectedCount() == 0) {
        led.setPattern(LedPattern::DOUBLE_FLASH);
    } else if (uploader.isUploading()) {
        led.setPattern(LedPattern::FAST_BLINK);
    } else if (!locationValid) {
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

        // Compute last upload string — prefer persisted epoch (survives reboot)
        String lastUpStr = "Never";
        unsigned long lastUpMillis = uploader.getLastUploadTime();
        time_t lastUpEpoch = uploader.getLastUploadEpoch();
        if (lastUpMillis > 0) {
            unsigned long ago = (millis() - lastUpMillis) / 1000;
            if (ago < 60) lastUpStr = String(ago) + "s ago";
            else if (ago < 3600) lastUpStr = String(ago / 60) + "m ago";
            else lastUpStr = String(ago / 3600) + "h ago";
        } else if (lastUpEpoch > 0) {
            time_t now = time(nullptr);
            if (now > 1000000000) {
                long ago = now - lastUpEpoch;
                if (ago < 60) lastUpStr = String(ago) + "s ago";
                else if (ago < 3600) lastUpStr = String(ago / 60) + "m ago";
                else if (ago < 86400) lastUpStr = String(ago / 3600) + "h ago";
                else lastUpStr = String(ago / 86400) + "d ago";
            }
        }

        DisplayStatus ds;
        ds.rcConnected    = radiaCodeMgr.connectedCount() > 0;
        ds.rcIsUsb        = radiaCodeMgr.isUsbConnected();
        ds.wifiConnected  = wifi.isSTAConnected();
        ds.wifiSSID       = wifi.getSSID();
        ds.staIP          = wifi.getSTAIP().toString();
        ds.lastUpload     = lastUpStr;
        ds.totalReadings  = stats.depth;
        ds.pendingReadings = stats.pending;
        ds.totalReadingsLogged = configMgr.getTotalReadingsLogged();
        ds.doseRate       = dose_rate;
        ds.countRate      = count_rate;
        ds.batteryVoltage = battery.getVoltage();
        ds.batteryPercent = battery.getPercent();
        ds.gpsFix         = locationValid;
        ds.gpsSats        = gps.getSatellites();
        ds.timeSyncSource = timeSyncSource;
        ds.uploadEnabled  = configMgr.getToken().length() > 0;
        ds.loopCount      = loopCounter;

        display.draw(ds);
    }
#endif

    // --- 8. Performance metrics -----------------------------------------------
    {
        unsigned long elapsed = micros() - loopStartUs;
        perfStats.loopDurationUs = elapsed;
        if (elapsed > perfStats.loopMaxUs) perfStats.loopMaxUs = elapsed;
        // EMA with alpha ~0.1 (smooth over ~10 loops)
        perfStats.loopAvgUs = (perfStats.loopAvgUs == 0)
            ? static_cast<float>(elapsed)
            : perfStats.loopAvgUs * 0.9f + static_cast<float>(elapsed) * 0.1f;
        // CPU usage = work time / (work time + sleep time)
        unsigned long intervalUs = configMgr.getReadingIntervalMs() * 1000UL;
        perfStats.cpuUsagePct = (intervalUs > 0)
            ? (perfStats.loopAvgUs / (perfStats.loopAvgUs + static_cast<float>(intervalUs))) * 100.0f
            : 0.0f;
        perfStats.freeHeap    = ESP.getFreeHeap();
        perfStats.minFreeHeap = ESP.getMinFreeHeap();
        perfStats.cpuFreqMHz  = static_cast<uint8_t>(getCpuFrequencyMhz());
    }
    portal.updatePerf(perfStats.loopAvgUs, perfStats.loopMaxUs, perfStats.cpuUsagePct,
                      perfStats.freeHeap, perfStats.minFreeHeap, perfStats.cpuFreqMHz);

    // --- 9. Wait until next reading interval ---------------------------------
    delay(configMgr.getReadingIntervalMs());
}

#include "portal.h"
#include "../config.h"
#include "../shipping_mode.h"
#include "debug_ws.h"
#include "html/dashboard_html.h"
#include "html/debug_html.h"
#include "html/settings_html.h"
#include "html/data_html.h"
#include "html/selftest_html.h"
#include "html/templates_js.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <NimBLEDevice.h>

// =============================================================================
// Shutdown flag — set by POST /api/actions/shutdown, read in main loop
// =============================================================================
volatile bool g_shutdownRequested = false;
volatile bool g_displayTestRequested = false;

// --- BLE scan state (shared between request handlers and scan task) ----------
struct BleScanEntry {
    String name;
    String mac;
    int    rssi;
};
static volatile bool _bleScanRunning = false;
static volatile bool _bleScanDone    = false;
static std::vector<BleScanEntry> _bleScanResults;

// =============================================================================
// RadiaLog Firmware - StatusPortal Implementation
// Async web server: dashboard, debug console, settings, data, OTA, and API.
// =============================================================================

StatusPortal::StatusPortal()
    : _server(nullptr)
    , _cfg(nullptr)
    , _buf(nullptr)
    , _rc(nullptr)
    , _gps(nullptr)
    , _wifi(nullptr)
    , _uploader(nullptr)
    , _doseRate(0.0f)
    , _countRate(0.0f)
    , _batteryVoltage(0.0f)
    , _batteryPercent(0)
    , _timeSyncSource("")
    , _perfLoopAvgUs(0)
    , _perfLoopMaxUs(0)
    , _perfCpuPct(0)
    , _perfFreeHeap(0)
    , _perfMinFreeHeap(0)
    , _perfCpuMHz(0)
{
}

void StatusPortal::begin(ConfigMgr& cfg, ReadingBuffer& buf, radiacode::RadiaCode& rc,
                         GPS& gps, WifiMgr& wifi, Uploader& uploader,
                         radiacode::RadiaCodeMgr* rcMgr) {
    _cfg      = &cfg;
    _buf      = &buf;
    _rc       = &rc;
    _gps      = &gps;
    _wifi     = &wifi;
    _uploader = &uploader;
    _rcMgr    = rcMgr;

    _server = new AsyncWebServer(80);

    // Register the debug WebSocket handler (created earlier in setup)
    AsyncWebSocket* ws = debugWS.getSocket();
    if (ws) {
        _server->addHandler(ws);
    }

    _registerRoutes();
    _server->begin();
}

void StatusPortal::updateReading(float doseRate, float countRate) {
    _doseRate  = doseRate;
    _countRate = countRate;
}

void StatusPortal::updateBattery(float voltage, uint8_t percent) {
    _batteryVoltage = voltage;
    _batteryPercent = percent;
}

void StatusPortal::setTimeSyncSource(const String& source) {
    _timeSyncSource = source;
}

void StatusPortal::updatePerf(float loopAvgUs, unsigned long loopMaxUs, float cpuPct,
                              uint32_t freeHeap, uint32_t minFreeHeap, uint8_t cpuMHz) {
    _perfLoopAvgUs   = loopAvgUs;
    _perfLoopMaxUs   = loopMaxUs;
    _perfCpuPct      = cpuPct;
    _perfFreeHeap    = freeHeap;
    _perfMinFreeHeap = minFreeHeap;
    _perfCpuMHz      = cpuMHz;
}

// =============================================================================
// Route Registration
// =============================================================================

void StatusPortal::_registerRoutes() {
    // --- HTML pages ---

    _server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", DASHBOARD_HTML);
    });

    _server->on("/debug", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", DEBUG_HTML);
    });

    _server->on("/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", SETTINGS_HTML);
    });

    _server->on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", DATA_HTML);
    });

    _server->on("/self-test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", SELFTEST_HTML);
    });

    _server->on("/templates.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "application/javascript", TEMPLATES_JS);
        response->addHeader("Cache-Control", "public, max-age=3600");
        request->send(response);
    });

    // --- API endpoints ---

    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleApiStatus(request);
    });

    _server->on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleApiSettings(request);
    });

    // POST /api/settings — raw body handler for JSON
    _server->on("/api/settings", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0 && total < 4096) {
                request->_tempObject = new String();
                static_cast<String*>(request->_tempObject)->reserve(total);
            }
            if (request->_tempObject == nullptr) return;

            String* bodyStr = static_cast<String*>(request->_tempObject);
            for (size_t i = 0; i < len; i++) {
                *bodyStr += static_cast<char>(data[i]);
            }

            if (index + len >= total) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, *bodyStr);
                delete bodyStr;
                request->_tempObject = nullptr;

                if (err) {
                    request->send(400, "application/json",
                        "{\"success\":false,\"error\":\"Invalid JSON\"}");
                    return;
                }

                JsonObject body = doc.as<JsonObject>();

                if (body["wifi"].is<JsonArray>()) {
                    while (_cfg->getWifiCount() > 0) {
                        _cfg->removeWifi(0);
                    }
                    JsonArray wifiArr = body["wifi"].as<JsonArray>();
                    int i = 0;
                    for (JsonObject net : wifiArr) {
                        if (i >= ConfigMgr::MAX_WIFI) break;
                        _cfg->setWifi(i, net["ssid"].as<String>(),
                                         net["password"].as<String>());
                        i++;
                    }
                }

                if (body["token"].is<const char*>())
                    _cfg->setToken(body["token"].as<String>());
                if (body["upload_url"].is<const char*>())
                    _cfg->setUploadUrl(body["upload_url"].as<String>());
                if (body["device_id"].is<const char*>())
                    _cfg->setDeviceId(body["device_id"].as<String>());
                if (body["device_name"].is<const char*>())
                    _cfg->setDeviceName(body["device_name"].as<String>());
                if (body["ap_password"].is<const char*>())
                    _cfg->setApPassword(body["ap_password"].as<String>());
                if (body["reading_interval_ms"].is<uint32_t>())
                    _cfg->setReadingIntervalMs(body["reading_interval_ms"].as<uint32_t>());
                if (body["google_api_key"].is<const char*>())
                    _cfg->setGoogleApiKey(body["google_api_key"].as<String>());
                if (body["display_timeout_sec"].is<int>())
                    _cfg->setDisplayTimeoutSec(body["display_timeout_sec"].as<uint16_t>());
                if (body["button_wake"].is<bool>())
                    _cfg->setButtonWakeEnabled(body["button_wake"].as<bool>());
                if (body["ble_devices"].is<JsonArray>()) {
                    std::vector<String> macs;
                    JsonArray bleArr = body["ble_devices"].as<JsonArray>();
                    for (JsonVariant v : bleArr) {
                        if (v.is<const char*>()) {
                            macs.push_back(v.as<String>());
                        }
                    }
                    _cfg->setBleDevices(macs);
                }

                bool ok = _cfg->save();

                // Apply new WiFi credentials immediately (without reboot)
                if (ok && _wifi) {
                    std::vector<WifiCredentials> networks;
                    for (int w = 0; w < _cfg->getWifiCount(); w++) {
                        WifiCredentials cred;
                        cred.ssid     = _cfg->getWifiSSID(w);
                        cred.password = _cfg->getWifiPass(w);
                        cred.priority = static_cast<uint8_t>(w);
                        networks.push_back(cred);
                    }
                    if (!networks.empty()) {
                        _wifi->setNetworks(networks);
                        _wifi->connectSTA();
                        debugWS.log(MOD_WIFI, LVL_INFO,
                            "[WiFi] Credentials updated, connecting to "
                            + networks[0].ssid + "...");
                    }
                }

                // Apply new token/URL to uploader immediately
                if (ok && _uploader) {
                    UploadConfig ucfg;
                    ucfg.radiamaps_url = _cfg->getUploadUrl();
                    ucfg.device_token  = _cfg->getToken();
                    ucfg.device_id     = _cfg->getDeviceId();
                    _uploader->updateConfig(ucfg);
                }

                // Apply new BLE device list immediately (without reboot)
                if (ok && _rcMgr) {
                    std::vector<String> bleMacs;
                    for (int b = 0; b < _cfg->getBleDeviceCount(); b++) {
                        bleMacs.push_back(_cfg->getBleDeviceMac(b));
                    }
                    _rcMgr->updateBleDevices(bleMacs);
                }

                JsonDocument resp;
                resp["success"] = ok;
                if (!ok) resp["error"] = "Failed to write config file";
                String out;
                serializeJson(resp, out);
                request->send(200, "application/json", out);
            }
        }
    );

    _server->on("/api/readings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleApiReadings(request);
    });

    _server->on("/api/readings/csv", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleApiReadingsCsv(request);
    });

    // --- OTA firmware upload ---

    _server->on("/api/ota", HTTP_POST,
        // Completion handler — runs after upload finishes
        [this](AsyncWebServerRequest* request) {
            bool ok = !Update.hasError();
            String resp = ok
                ? "{\"success\":true,\"message\":\"Update successful, rebooting...\"}"
                : "{\"success\":false,\"error\":\"Update failed\"}";
            request->send(200, "application/json", resp);
            if (ok) {
                _cfg->flushTotalReadingsLogged();
                delay(500);
                ESP.restart();
            }
        },
        // Upload handler — streams firmware chunks to flash
        [](AsyncWebServerRequest* request, String filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                Serial.printf("[OTA] Begin: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (Update.isRunning()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Complete: %u bytes\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // --- Action endpoints ---

    _server->on("/api/actions/upload", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _uploader->forceUpload();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Upload triggered\"}");
    });

    _server->on("/api/actions/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _cfg->flushTotalReadingsLogged();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });

    _server->on("/api/actions/shutdown", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json",
            "{\"success\":true,\"message\":\"Entering shipping mode...\"}");
        g_shutdownRequested = true;
    });

    _server->on("/api/actions/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        LittleFS.remove("/readings.bin");
        LittleFS.remove("/readings_status.bin");
        LittleFS.remove("/readings_idx.bin");
        _buf->begin();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Buffer cleared\"}");
    });

    _server->on("/api/actions/display-test", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifdef HAS_DISPLAY
        g_displayTestRequested = true;
        request->send(200, "application/json",
            "{\"success\":true,\"has_display\":true,\"message\":\"Display wake requested\"}");
#else
        request->send(200, "application/json",
            "{\"success\":false,\"has_display\":false,\"message\":\"No display on this board\"}");
#endif
    });

    _server->on("/api/actions/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        LittleFS.remove("/readings.bin");
        LittleFS.remove("/readings_status.bin");
        LittleFS.remove("/readings_idx.bin");
        _cfg->factoryReset();
        request->send(200, "application/json",
            "{\"success\":true,\"message\":\"Factory reset complete, rebooting...\"}");
        delay(500);
        ESP.restart();
    });

    // POST /api/actions/verify-token — verify a device token against RadiaMaps
    _server->on("/api/actions/verify-token", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0 && total < 4096) {
                request->_tempObject = new String();
                static_cast<String*>(request->_tempObject)->reserve(total);
            }
            if (request->_tempObject == nullptr) return;

            String* bodyStr = static_cast<String*>(request->_tempObject);
            for (size_t i = 0; i < len; i++) {
                *bodyStr += static_cast<char>(data[i]);
            }

            if (index + len < total) return;  // wait for full body

            String body = *bodyStr;
            delete bodyStr;
            request->_tempObject = nullptr;

            JsonDocument reqDoc;
            if (deserializeJson(reqDoc, body)) {
                request->send(400, "application/json",
                    "{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }

            String token = reqDoc["token"].as<String>();
            if (token.length() == 0) {
                request->send(400, "application/json",
                    "{\"success\":false,\"error\":\"Token is empty\"}");
                return;
            }

            // Derive verify URL from configured upload URL by replacing trailing /upload with /verify
            String verifyUrl = _cfg->getUploadUrl();
            if (verifyUrl.endsWith("/upload")) {
                verifyUrl = verifyUrl.substring(0, verifyUrl.length() - 7) + "/verify";
            }

            if (verifyUrl.length() == 0) {
                request->send(400, "application/json",
                    "{\"success\":false,\"error\":\"No upload URL configured\"}");
                return;
            }

            debugWS.log(MOD_UPLOAD, LVL_INFO,
                "[Portal] Verifying token via " + verifyUrl);

            WiFiClientSecure client;
            client.setInsecure();
            client.setTimeout(15);

            HTTPClient http;
            http.setConnectTimeout(10000);
            http.setTimeout(15000);
            http.begin(client, verifyUrl);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("X-Device-Token", token);

            int httpCode = http.POST("");
            String respOut;

            if (httpCode == HTTP_CODE_OK) {
                String response = http.getString();
                JsonDocument respDoc;
                DeserializationError err = deserializeJson(respDoc, response);
                if (err == DeserializationError::Ok && respDoc["username"].is<const char*>()) {
                    JsonDocument out;
                    out["success"]      = true;
                    out["username"]     = respDoc["username"];
                    out["subscription"] = respDoc["subscription_status"];
                    serializeJson(out, respOut);
                } else {
                    respOut = "{\"success\":false,\"error\":\"Unexpected server response\"}";
                }
            } else if (httpCode == 401 || httpCode == 403) {
                respOut = "{\"success\":false,\"error\":\"Invalid token\"}";
            } else if (httpCode > 0) {
                respOut = "{\"success\":false,\"error\":\"Server returned HTTP " + String(httpCode) + "\"}";
            } else {
                respOut = "{\"success\":false,\"error\":\"Connection failed — check WiFi\"}";
            }

            http.end();

            debugWS.log(MOD_UPLOAD, LVL_INFO,
                "[Portal] Token verify result: " + respOut.substring(0, 200));

            request->send(200, "application/json", respOut);
        }
    );

    // --- BLE scan endpoints (async to avoid watchdog reset) ---
    // POST /api/ble/scan  — kick off a background scan
    // GET  /api/ble/scan  — poll for results

    _server->on("/api/ble/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (_bleScanRunning) {
            debugWS.log(MOD_BLE, LVL_WARN, "[BLE] Scan already running, ignoring request");
            request->send(200, "application/json",
                "{\"status\":\"scanning\"}");
            return;
        }

        debugWS.log(MOD_BLE, LVL_INFO, "[BLE] Scan requested");

        if (!NimBLEDevice::getInitialized()) {
            debugWS.log(MOD_BLE, LVL_INFO, "[BLE] Initializing NimBLE stack...");
            NimBLEDevice::init("RadiaLog");
            debugWS.log(MOD_BLE, LVL_INFO, "[BLE] NimBLE initialized");
        } else {
            debugWS.log(MOD_BLE, LVL_DEBUG, "[BLE] NimBLE already initialized");
        }

        _bleScanResults.clear();
        _bleScanRunning = true;
        _bleScanDone = false;

        // Launch scan in a one-shot FreeRTOS task so we don't block
        // the async web server (which would trigger a watchdog reset).
        xTaskCreatePinnedToCore(
            [](void*) {
                debugWS.log(MOD_BLE, LVL_INFO, "[BLE] Scan task started on core "
                    + String(xPortGetCoreID()));

                NimBLEScan* scan = NimBLEDevice::getScan();
                if (!scan) {
                    debugWS.log(MOD_BLE, LVL_ERROR, "[BLE] getScan() returned null!");
                    _bleScanDone = true;
                    _bleScanRunning = false;
                    vTaskDelete(nullptr);
                    return;
                }

                // Stop any in-progress scan first
                debugWS.log(MOD_BLE, LVL_DEBUG, "[BLE] Stopping previous scan...");
                scan->stop();
                delay(100);

                scan->setActiveScan(true);
                scan->setInterval(100);
                scan->setWindow(99);

                debugWS.log(MOD_BLE, LVL_INFO,
                    "[BLE] Starting active scan (5s, interval=100, window=99)...");
                NimBLEScanResults results = scan->start(5, false);

                int total = results.getCount();
                debugWS.log(MOD_BLE, LVL_INFO,
                    "[BLE] Scan complete. Total devices found: " + String(total));

                // RadiaCode BLE service UUID — match devices even if name is missing
                static const NimBLEUUID RC_SVC("e63215e5-7003-49d8-96b0-b024798fb901");

                _bleScanResults.clear();
                for (int i = 0; i < total; i++) {
                    NimBLEAdvertisedDevice dev = results.getDevice(i);
                    String mac  = dev.getAddress().toString().c_str();
                    String name = dev.getName().c_str();
                    int rssi    = dev.getRSSI();
                    bool hasName = name.length() > 0;
                    bool nameMatch = name.startsWith("RC-");
                    bool svcMatch  = dev.isAdvertisingService(RC_SVC);

                    // Log every device with full details
                    String svcList = "";
                    if (dev.getServiceUUIDCount() > 0) {
                        for (int s = 0; s < dev.getServiceUUIDCount(); s++) {
                            if (s > 0) svcList += ", ";
                            svcList += dev.getServiceUUID(s).toString().c_str();
                        }
                    } else {
                        svcList = "(none)";
                    }

                    debugWS.log(MOD_BLE, LVL_INFO,
                        "[BLE] Device " + String(i) + ": mac=" + mac
                        + " name=" + (hasName ? ("\"" + name + "\"") : "(empty)")
                        + " rssi=" + String(rssi)
                        + " svcs=" + svcList
                        + " nameMatch=" + String(nameMatch ? "Y" : "N")
                        + " svcMatch=" + String(svcMatch ? "Y" : "N"));

                    if (nameMatch || svcMatch) {
                        BleScanEntry entry;
                        entry.name = hasName ? name
                                   : ("RC-" + mac);
                        entry.mac  = mac;
                        entry.rssi = rssi;
                        _bleScanResults.push_back(entry);
                        debugWS.log(MOD_BLE, LVL_INFO,
                            "[BLE] >> MATCHED as RadiaCode: " + entry.name);
                    }
                }
                scan->clearResults();

                debugWS.log(MOD_BLE, LVL_INFO,
                    "[BLE] Scan done. " + String(_bleScanResults.size())
                    + " RadiaCode device(s) matched out of " + String(total));

                _bleScanDone = true;
                _bleScanRunning = false;
                vTaskDelete(nullptr);
            },
            "ble_scan", 8192, nullptr, 1, nullptr, 0  // core 0
        );

        request->send(200, "application/json", "{\"status\":\"scanning\"}");
    });

    _server->on("/api/ble/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;

        if (_bleScanRunning) {
            doc["status"] = "scanning";
        } else if (_bleScanDone) {
            doc["status"] = "done";
            JsonArray arr = doc["devices"].to<JsonArray>();
            for (const auto& entry : _bleScanResults) {
                JsonObject obj = arr.add<JsonObject>();
                obj["name"] = entry.name;
                obj["mac"]  = entry.mac;
                obj["rssi"] = entry.rssi;
            }
        } else {
            doc["status"] = "idle";
        }

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // --- 404 fallback ---
    _server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });
}

// =============================================================================
// GET /api/status — JSON snapshot of all subsystem state
// =============================================================================

void StatusPortal::_handleApiStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    // Firmware
    doc["fw_version"] = FW_VERSION;
    doc["uptime"]     = millis() / 1000;

    // Radiation
    doc["dose_rate"]  = _doseRate;
    doc["count_rate"] = _countRate;

    // GPS
    doc["gps_fix"]  = _gps->hasFix();
    doc["gps_lat"]  = _gps->getLat();
    doc["gps_lon"]  = _gps->getLon();
    doc["gps_sats"] = _gps->getSatellites();

    // WiFi
    doc["wifi_connected"] = _wifi->isSTAConnected();
    doc["wifi_rssi"]      = _wifi->getSignalStrength();
    doc["wifi_ssid"]      = _wifi->getSSID();
    doc["wifi_sta_ip"]    = _wifi->getSTAIP().toString();

    // RadiaCode — USB priority, BLE fallback
    bool usbOk = _rcMgr ? _rcMgr->isUsbConnected() : _rc->isConnected();
    bool bleOk = _rcMgr ? (_rcMgr->bleConnectedCount() > 0) : false;
    doc["usb_connected"] = usbOk;
    doc["ble_connected"] = bleOk;
    doc["rc_connected"]  = usbOk || bleOk;
    doc["rc_source"]     = usbOk ? "USB" : (bleOk ? "BLE" : "None");

    // Buffer
    BufferStats stats = _buf->getStats();
    doc["buffer_pending"] = stats.pending;
    doc["buffer_total"]   = stats.depth;

    // Upload — send last_upload_epoch for client-side formatting
    unsigned long lastUpMillis = _uploader->getLastUploadTime();
    time_t lastUpEpoch = _uploader->getLastUploadEpoch();
    if (lastUpMillis > 0) {
        // Use millis-based timestamp converted to epoch for consistency
        time_t nowEpoch = time(nullptr);
        unsigned long agoSec = (millis() - lastUpMillis) / 1000;
        if (nowEpoch > 1000000000) {
            doc["last_upload_epoch"] = static_cast<unsigned long>(nowEpoch - agoSec);
        }
        doc["last_upload"] = "recent";
    } else if (lastUpEpoch > 0) {
        doc["last_upload_epoch"] = static_cast<unsigned long>(lastUpEpoch);
        doc["last_upload"] = "past";
    } else {
        doc["last_upload"] = "Never";
    }

    // Next scheduled upload
    time_t nextUp = _uploader->getNextUploadEpoch();
    if (nextUp > 0) {
        doc["next_upload_epoch"] = static_cast<unsigned long>(nextUp);
    }

    // Battery
    doc["battery_voltage"] = _batteryVoltage;
    doc["battery_percent"] = _batteryPercent;

    // Performance
    doc["cpu_mhz"]        = _perfCpuMHz;
    doc["cpu_usage_pct"]  = serialized(String(_perfCpuPct, 1));
    doc["loop_avg_ms"]    = serialized(String(_perfLoopAvgUs / 1000.0f, 1));
    doc["loop_max_ms"]    = serialized(String(_perfLoopMaxUs / 1000.0f, 1));
    doc["free_heap"]      = _perfFreeHeap;
    doc["min_free_heap"]  = _perfMinFreeHeap;

    // Disk (LittleFS)
    doc["disk_total"]  = LittleFS.totalBytes();
    doc["disk_used"]   = LittleFS.usedBytes();

    // GPS health
    doc["gps_sentences_fix"]  = _gps->sentencesWithFix();
    doc["gps_failed_cksum"]   = _gps->failedChecksums();
    doc["gps_chars_processed"] = _gps->charsProcessed();

    // WiFi AP
    doc["ap_active"] = _wifi->isAPActive();

    // Boot button press counter (used by self-test page to confirm button works)
    doc["button_press_count"] = g_buttonPressCount;

    // Time
    doc["time_synced"] = _timeSyncSource.length() > 0;
    doc["time_sync_source"] = _timeSyncSource;
    time_t now = time(nullptr);
    if (now > 1000000000) {
        doc["system_time"] = static_cast<unsigned long>(now);
    }

    // Lifetime readings logged (gamification counter)
    doc["total_readings_logged"] = _cfg->getTotalReadingsLogged();

    // Upload enabled — false when no device token is configured
    doc["upload_enabled"] = _cfg->getToken().length() > 0;

    // Display capabilities (so the web UI can conditionally show display settings)
#ifdef HAS_DISPLAY
    doc["has_display"] = true;
#ifdef DISPLAY_CIRCULAR
    doc["display_shape"] = "circular";
#else
    doc["display_shape"] = "rectangular";
#endif
    doc["display_width"]  = DISPLAY_WIDTH;
    doc["display_height"] = DISPLAY_HEIGHT;
#else
    doc["has_display"] = false;
#endif

    // RadiaMaps account (cached)
    const AccountInfo& acct = _uploader->getAccountInfo();
    if (acct.valid) {
        doc["rm_username"]          = acct.username;
        doc["rm_subscription"]      = acct.subscription;
        if (acct.lifetime_readings >= 0) {
            doc["rm_lifetime_readings"] = acct.lifetime_readings;
        }
        if (acct.last_queried > 0) {
            doc["rm_last_queried"] = static_cast<unsigned long>(acct.last_queried);
        }
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

// =============================================================================
// GET /api/settings — current config as JSON
// =============================================================================

void StatusPortal::_handleApiSettings(AsyncWebServerRequest* request) {
    JsonDocument doc;

    JsonArray wifiArr = doc["wifi"].to<JsonArray>();
    for (int i = 0; i < _cfg->getWifiCount(); i++) {
        JsonObject net = wifiArr.add<JsonObject>();
        net["ssid"]     = _cfg->getWifiSSID(i);
        net["password"] = _cfg->getWifiPass(i);
    }

    doc["token"]               = _cfg->getToken();
    doc["upload_url"]          = _cfg->getUploadUrl();
    doc["device_id"]           = _cfg->getDeviceId();
    doc["device_name"]         = _cfg->getDeviceName();
    doc["reading_interval_ms"] = _cfg->getReadingIntervalMs();
    doc["ap_password"]         = _cfg->getApPassword();
    doc["google_api_key"]      = _cfg->getGoogleApiKey();
    doc["display_timeout_sec"] = _cfg->getDisplayTimeoutSec();
    doc["button_wake"]         = _cfg->getButtonWakeEnabled();

    JsonArray bleArr = doc["ble_devices"].to<JsonArray>();
    for (int i = 0; i < _cfg->getBleDeviceCount(); i++) {
        bleArr.add(_cfg->getBleDeviceMac(i));
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

// =============================================================================
// GET /api/readings — last 100 readings as JSON
// =============================================================================

void StatusPortal::_handleApiReadings(AsyncWebServerRequest* request) {
    static const uint32_t MAX_DISPLAY = 100;

    BufferStats stats = _buf->getStats();
    uint32_t total = stats.depth;
    uint32_t toRead = (total < MAX_DISPLAY) ? total : MAX_DISPLAY;

    JsonDocument doc;
    JsonArray arr = doc["readings"].to<JsonArray>();

    if (toRead > 0) {
        File rf = LittleFS.open("/readings.bin", "r");
        if (rf) {
            size_t startOffset = (total - toRead) * READING_BINARY_SIZE;
            rf.seek(startOffset);

            for (uint32_t i = 0; i < toRead; i++) {
                uint8_t record[READING_BINARY_SIZE];
                if (rf.read(record, READING_BINARY_SIZE) != READING_BINARY_SIZE) break;

                Reading r;
                size_t off = 0;
                memcpy(&r.lat,        &record[off], 4); off += 4;
                memcpy(&r.lon,        &record[off], 4); off += 4;
                memcpy(&r.dose_rate,  &record[off], 4); off += 4;
                memcpy(&r.count_rate, &record[off], 4); off += 4;
                memcpy(&r.timestamp,  &record[off], 4); off += 4;
                memcpy(&r.accuracy,   &record[off], 4); off += 4;
                memcpy(&r.altitude,   &record[off], 4); off += 4;
                uint16_t u16;
                memcpy(&u16, &record[off], 2); off += 2; r.speed_mph = u16 / 10.0f;
                memcpy(&u16, &record[off], 2); off += 2; r.speed_kph = u16 / 10.0f;
                memcpy(&u16, &record[off], 2); off += 2; r.heading   = u16 / 10.0f;

                JsonObject obj = arr.add<JsonObject>();
                obj["id"]         = total - toRead + i;
                obj["timestamp"]  = r.timestamp;
                obj["dose_rate"]  = r.dose_rate;
                obj["count_rate"] = r.count_rate;
                obj["lat"]        = r.lat;
                obj["lon"]        = r.lon;
                obj["altitude"]   = r.altitude;
                obj["speed_kph"]  = r.speed_kph;
                obj["heading"]    = r.heading;
                obj["accuracy"]   = r.accuracy;
            }
            rf.close();
        }
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

// =============================================================================
// GET /api/readings/csv — export all readings as CSV download
// =============================================================================

void StatusPortal::_handleApiReadingsCsv(AsyncWebServerRequest* request) {
    BufferStats stats = _buf->getStats();
    uint32_t total = stats.depth;

    AsyncResponseStream* stream = request->beginResponseStream("text/csv");
    stream->addHeader("Content-Disposition", "attachment; filename=radialog_readings.csv");

    stream->print("id,timestamp,dose_rate,count_rate,lat,lon,altitude,speed_kph,heading,accuracy\n");

    if (total > 0) {
        File rf = LittleFS.open("/readings.bin", "r");
        if (rf) {
            for (uint32_t i = 0; i < total; i++) {
                uint8_t record[READING_BINARY_SIZE];
                if (rf.read(record, READING_BINARY_SIZE) != READING_BINARY_SIZE) break;

                float lat, lon, dose, count, acc, alt;
                uint32_t ts;
                uint16_t spdK, hdg;
                size_t off = 0;
                memcpy(&lat,   &record[off], 4); off += 4;
                memcpy(&lon,   &record[off], 4); off += 4;
                memcpy(&dose,  &record[off], 4); off += 4;
                memcpy(&count, &record[off], 4); off += 4;
                memcpy(&ts,    &record[off], 4); off += 4;
                memcpy(&acc,   &record[off], 4); off += 4;
                memcpy(&alt,   &record[off], 4); off += 4;
                off += 2;
                memcpy(&spdK,  &record[off], 2); off += 2;
                memcpy(&hdg,   &record[off], 2); off += 2;

                char line[160];
                snprintf(line, sizeof(line),
                    "%u,%u,%.4f,%.1f,%.6f,%.6f,%.1f,%.1f,%.0f,%.1f\n",
                    i, ts, dose, count, lat, lon, alt,
                    spdK / 10.0f, hdg / 10.0f, acc);
                stream->print(line);
            }
            rf.close();
        }
    }

    request->send(stream);
}

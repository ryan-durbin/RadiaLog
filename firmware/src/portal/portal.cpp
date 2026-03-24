#include "portal.h"
#include "debug_ws.h"
#include "html/dashboard_html.h"
#include "html/debug_html.h"
#include "html/settings_html.h"
#include "html/data_html.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>

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
    , _timeSynced(false)
{
}

void StatusPortal::begin(ConfigMgr& cfg, ReadingBuffer& buf, radiacode::RadiaCode& rc,
                         GPS& gps, WifiMgr& wifi, Uploader& uploader) {
    _cfg      = &cfg;
    _buf      = &buf;
    _rc       = &rc;
    _gps      = &gps;
    _wifi     = &wifi;
    _uploader = &uploader;

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

void StatusPortal::setTimeSynced(bool synced) {
    _timeSynced = synced;
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

                bool ok = _cfg->save();

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
        [](AsyncWebServerRequest* request) {
            bool ok = !Update.hasError();
            String resp = ok
                ? "{\"success\":true,\"message\":\"Update successful, rebooting...\"}"
                : "{\"success\":false,\"error\":\"Update failed\"}";
            request->send(200, "application/json", resp);
            if (ok) {
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

    _server->on("/api/actions/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });

    _server->on("/api/actions/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        LittleFS.remove("/readings.bin");
        LittleFS.remove("/readings_status.bin");
        LittleFS.remove("/readings_idx.bin");
        _buf->begin();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Buffer cleared\"}");
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

    // USB
    doc["usb_connected"] = _rc->isConnected();

    // Buffer
    BufferStats stats = _buf->getStats();
    doc["buffer_pending"] = stats.pending;
    doc["buffer_total"]   = stats.depth;

    // Upload
    unsigned long lastUp = _uploader->getLastUploadTime();
    if (lastUp > 0) {
        unsigned long ago = (millis() - lastUp) / 1000;
        doc["last_upload"] = String(ago) + "s ago";
    } else {
        doc["last_upload"] = "Never";
    }

    // Battery
    doc["battery_voltage"] = _batteryVoltage;
    doc["battery_percent"] = _batteryPercent;

    // Time
    doc["time_synced"] = _timeSynced;
    time_t now = time(nullptr);
    if (now > 1000000000) {
        doc["system_time"] = static_cast<unsigned long>(now);
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

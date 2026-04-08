#include "uploader.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include "portal/debug_ws.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

// =============================================================================
// RadiaLog Firmware - Upload Manager Implementation
// Uploads once per day at midnight UTC + random jitter (0-59 min).
// On WiFi connect, uploads immediately if >24h since last upload or if there
// is a backlog of pending readings. Drains all pending readings in batches.
// =============================================================================

static const char* LAST_UPLOAD_PATH  = "/last_upload.json";
static const char* ACCOUNT_INFO_PATH = "/account_info.json";

Uploader::Uploader()
    : _buffer(nullptr)
    , _wifi(nullptr)
    , _uploading(false)
    , _lastUploadTime(0)
    , _forceFlag(false)
    , _taskHandle(nullptr)
    , _backoffMs(BACKOFF_INITIAL_MS)
    , _lastUploadEpoch(0)
    , _jitterMinutes(0)
    , _uploadDueOnConnect(false)
    , _serverUnreachableUntilMs(0)
    , _lastAccountFetchMs(0)
    , _accountRefreshFlag(false)
{
}

void Uploader::begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi) {
    _config = config;
    _buffer = buffer;
    _wifi   = wifi;

    // Random jitter: 0-59 minutes past midnight UTC for daily upload
    _jitterMinutes = static_cast<uint8_t>(esp_random() % 60);

    // Load persisted state from LittleFS
    _loadLastUploadEpoch();
    _loadAccountInfo();

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] Daily upload jitter: " + String(_jitterMinutes) + " min past midnight UTC");
    if (_lastUploadEpoch > 0) {
        debugWS.log(MOD_UPLOAD, LVL_INFO,
            "[Uploader] Last upload epoch: " + String((uint32_t)_lastUploadEpoch));
    }

    xTaskCreatePinnedToCore(
        _uploadTask, "uploader", TASK_STACK_SIZE,
        this, TASK_PRIORITY, &_taskHandle, 0
    );
}

void Uploader::updateConfig(const UploadConfig& config) {
    _config = config;
    refreshAccountInfo();
}

void Uploader::forceUpload() {
    _forceFlag = true;
    if (_taskHandle != nullptr) {
        xTaskNotifyGive(_taskHandle);
    }
}

void Uploader::onWifiConnect() {
    // Check if upload is overdue (>24h or never uploaded with pending data)
    time_t now = time(nullptr);
    if (now > 1000000000) {
        bool overdue = (_lastUploadEpoch == 0) || (now - _lastUploadEpoch > 86400);
        if (overdue) {
            _uploadDueOnConnect = true;
            debugWS.log(MOD_UPLOAD, LVL_INFO,
                "[Uploader] WiFi connected, upload overdue — triggering upload.");
            forceUpload();
        }
    }
}

unsigned long Uploader::getLastUploadTime() const {
    return _lastUploadTime;
}

time_t Uploader::getLastUploadEpoch() const {
    return _lastUploadEpoch;
}

bool Uploader::isUploading() const {
    return _uploading;
}

time_t Uploader::getNextUploadEpoch() const {
    time_t now = time(nullptr);
    if (now < 1000000000) return 0;  // NTP not synced

    struct tm tmNow;
    gmtime_r(&now, &tmNow);

    // Today's target: midnight UTC + jitter
    struct tm tmTarget = tmNow;
    tmTarget.tm_hour = 0;
    tmTarget.tm_min  = _jitterMinutes;
    tmTarget.tm_sec  = 0;
    time_t todayTarget = mktime(&tmTarget);

    // If we haven't passed today's target yet, that's the next upload
    if (now < todayTarget) return todayTarget;

    // Otherwise, tomorrow's target
    tmTarget.tm_mday += 1;
    return mktime(&tmTarget);
}

const AccountInfo& Uploader::getAccountInfo() const {
    return _accountInfo;
}

void Uploader::refreshAccountInfo() {
    _accountRefreshFlag = true;
    if (_taskHandle != nullptr) {
        xTaskNotifyGive(_taskHandle);
    }
}

// =============================================================================
// Scheduling — determine if an upload should happen now
// =============================================================================

bool Uploader::_isUploadDue() const {
    time_t now = time(nullptr);
    if (now < 1000000000) return false;  // NTP not synced yet

    // If flagged by onWifiConnect (overdue), upload now
    if (_uploadDueOnConnect) return true;

    // Force flag from manual trigger
    if (_forceFlag) return true;

    // No pending readings — nothing to do
    if (_buffer == nullptr) return false;
    BufferStats stats = _buffer->getStats();
    if (stats.pending == 0) return false;

    // First upload is handled by onWifiConnect() which sets _uploadDueOnConnect
    // when the device has never uploaded.  No separate check needed here.

    // Check if we've crossed midnight UTC + jitter since last upload
    if (_lastUploadEpoch == 0) return false;  // wait for onWifiConnect trigger
    struct tm tmNow;
    gmtime_r(&now, &tmNow);

    // Today's upload window: midnight UTC + jitter minutes
    struct tm tmTarget = tmNow;
    tmTarget.tm_hour = 0;
    tmTarget.tm_min  = _jitterMinutes;
    tmTarget.tm_sec  = 0;
    time_t todayTarget = mktime(&tmTarget);
    // mktime interprets as local, but we fed it UTC fields.
    // Use timegm equivalent: since ESP32 TZ is UTC by default, this works.

    // If now is past today's target AND last upload was before today's target
    if (now >= todayTarget && _lastUploadEpoch < todayTarget) {
        return true;
    }

    return false;
}

// =============================================================================
// FreeRTOS Task
// =============================================================================

void Uploader::_uploadTask(void* pvParameters) {
    Uploader* self = static_cast<Uploader*>(pvParameters);

    for (;;) {
        // Wait for check interval or forceUpload() notification
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CHECK_INTERVAL_MS));

        bool forced = self->_forceFlag;
        self->_forceFlag = false;

        // Only attempt network operations if WiFi STA is connected
        if (self->_wifi != nullptr && self->_wifi->isSTAConnected()) {
            // Account info: refresh on flag, or every hour, or if never fetched
            bool needAccountRefresh = self->_accountRefreshFlag
                || !self->_accountInfo.valid
                || (self->_lastAccountFetchMs > 0 &&
                    (millis() - self->_lastAccountFetchMs) >= ACCOUNT_REFRESH_INTERVAL_MS);

            if (needAccountRefresh && self->_config.device_token.length() > 0) {
                self->_accountRefreshFlag = false;
                self->_fetchAccountInfo();
            }

            if (forced || self->_isUploadDue()) {
                // Skip uploads entirely if no device token is configured
                if (self->_config.device_token.length() == 0) {
                    if (forced) {
                        debugWS.log(MOD_UPLOAD, LVL_WARN,
                            "[Uploader] No device token configured. Uploads disabled.");
                    }
                } else {
                    // Manual force upload bypasses the server unreachable delay
                    if (forced) self->_serverUnreachableUntilMs = 0;

                    // If server was recently unreachable, wait until the delay expires
                    if (self->_serverUnreachableUntilMs > 0 && millis() < self->_serverUnreachableUntilMs) {
                        debugWS.log(MOD_UPLOAD, LVL_INFO,
                            "[Uploader] Server unreachable, retry in " +
                            String((self->_serverUnreachableUntilMs - millis()) / 60000UL) + " min.");
                    } else {
                        // Ping server before uploading to avoid hammering an offline server
                        debugWS.log(MOD_UPLOAD, LVL_INFO, "[Uploader] Pinging server...");
                        if (self->_pingServer()) {
                            self->_serverUnreachableUntilMs = 0;
                            self->_runUploadCycle();
                        } else {
                            self->_serverUnreachableUntilMs = millis() + SERVER_RETRY_DELAY_MS;
                            debugWS.log(MOD_UPLOAD, LVL_WARN,
                                "[Uploader] Server unreachable. Delaying uploads for 1 hour.");
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// Server reachability check — lightweight HEAD request before uploading
// =============================================================================

bool Uploader::_pingServer() {
    if (_config.radiamaps_url.length() == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.begin(client, _config.radiamaps_url);
    http.addHeader("X-Device-Token", _config.device_token);

    int httpCode = http.sendRequest("HEAD");
    http.end();

    // Any response from the server (even 4xx/5xx) means it's reachable
    return httpCode > 0;
}

// =============================================================================
// Upload Cycle — drain all pending readings in batches
// =============================================================================

void Uploader::_runUploadCycle() {
    if (_buffer == nullptr) return;

    BufferStats stats = _buffer->getStats();
    if (stats.pending == 0) {
        return;
    }

    // Verify WiFi is still connected before starting
    if (_wifi == nullptr || !_wifi->isSTAConnected()) {
        debugWS.log(MOD_UPLOAD, LVL_WARN,
            "[Uploader] WiFi not connected, skipping upload cycle.");
        return;
    }

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] Starting upload cycle. Pending: " + String(stats.pending));

    Reading* batch = new Reading[MAX_BATCH_SIZE];
    if (batch == nullptr) return;

    uint32_t totalUploaded = 0;

    for (;;) {
        // Check WiFi before each batch — bail out if connection dropped
        if (_wifi == nullptr || !_wifi->isSTAConnected()) {
            debugWS.log(MOD_UPLOAD, LVL_WARN,
                "[Uploader] WiFi lost mid-cycle after " + String(totalUploaded) + " readings. Will retry.");
            break;
        }

        uint32_t count = _buffer->getUnuploaded(batch, MAX_BATCH_SIZE);
        if (count == 0) break;

        uint32_t* ids = new uint32_t[count];
        if (ids == nullptr) break;

        // Build JSON payload in a nested scope so JsonDocument is freed
        // before _postBatch allocates SSL buffers (~40-50KB).
        String jsonPayload;
        {
            JsonDocument doc;
            JsonArray readings = doc["readings"].to<JsonArray>();

            for (uint32_t i = 0; i < count; i++) {
                JsonObject obj = readings.add<JsonObject>();
                obj["id"]               = batch[i].id;
                obj["reading_time"]     = (unsigned long long)batch[i].timestamp * 1000ULL;
                obj["latitude"]         = batch[i].lat;
                obj["longitude"]        = batch[i].lon;
                obj["dose_rate"]        = batch[i].dose_rate;
                obj["count_rate"]       = batch[i].count_rate;
                obj["altitude"]         = batch[i].altitude;
                obj["speed_mph"]        = batch[i].speed_mph;
                obj["speed_kph"]        = batch[i].speed_kph;
                obj["heading"]          = batch[i].heading;
                obj["accuracy"]         = batch[i].accuracy;
                obj["altitude_accuracy"] = batch[i].altitude_accuracy;
                ids[i] = batch[i].id;
            }

            serializeJson(doc, jsonPayload);
        } // JsonDocument freed here — reclaims heap before SSL allocation

        _uploading = true;
        bool success = _postBatch(
            reinterpret_cast<const uint8_t*>(jsonPayload.c_str()),
            jsonPayload.length()
        );
        _uploading = false;

        if (success) {
            // Only mark readings as uploaded after confirmed server success
            _buffer->markUploaded(ids, count);
            _lastUploadTime = millis();
            _backoffMs = BACKOFF_INITIAL_MS;
            totalUploaded += count;
            delete[] ids;
        } else {
            // Upload failed — do NOT mark or remove readings.
            // They remain pending in the buffer for the next attempt.
            delete[] ids;
            vTaskDelay(pdMS_TO_TICKS(_backoffMs));
            if (_backoffMs < BACKOFF_MAX_MS) {
                _backoffMs *= 2;
                if (_backoffMs > BACKOFF_MAX_MS) _backoffMs = BACKOFF_MAX_MS;
            }
            delete[] batch;
            debugWS.log(MOD_UPLOAD, LVL_WARN,
                "[Uploader] Batch failed after " + String(totalUploaded) + " readings uploaded. "
                "Remaining readings preserved in buffer. Will retry.");
            return;
        }
    }

    delete[] batch;

    if (totalUploaded > 0) {
        // Record successful upload time only after confirmed uploads
        _lastUploadEpoch = time(nullptr);
        _saveLastUploadEpoch();

        // Clear the on-connect flag only after successful upload
        _uploadDueOnConnect = false;

        // Prune only readings that were confirmed uploaded by the server
        _buffer->pruneUploaded();

        debugWS.log(MOD_UPLOAD, LVL_INFO,
            "[Uploader] Upload complete. " + String(totalUploaded) + " readings uploaded and pruned.");
    }
}

// =============================================================================
// HTTP POST
// =============================================================================

bool Uploader::_postBatch(const uint8_t* jsonPayload, size_t len) {
    if (_config.radiamaps_url.length() == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();   // Skip cert verification (ESP32 has no CA store)
    client.setTimeout(15);  // 15 second TLS timeout

    HTTPClient http;
    http.setConnectTimeout(10000);  // 10s connect timeout
    http.setTimeout(15000);         // 15s response timeout
    http.begin(client, _config.radiamaps_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _config.device_token);
    http.addHeader("X-Device-Id", _config.device_id);

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] POST " + _config.radiamaps_url + " (" + String(len) + " bytes)");

    int httpCode = http.POST(const_cast<uint8_t*>(jsonPayload), len);
    bool success = false;

    if (httpCode > 0) {
        String response = http.getString();
        debugWS.log(MOD_UPLOAD, LVL_INFO,
            "[Uploader] HTTP " + String(httpCode) + ": " + response.substring(0, 200));

        if (httpCode == HTTP_CODE_OK || httpCode == 202) {
            JsonDocument responseDoc;
            DeserializationError err = deserializeJson(responseDoc, response);
            if (err != DeserializationError::Ok) {
                debugWS.log(MOD_UPLOAD, LVL_ERROR,
                    "[Uploader] JSON parse error: " + String(err.c_str()));
            } else if (responseDoc["accepted"].is<int>()) {
                int accepted = responseDoc["accepted"].as<int>();
                success = accepted > 0;
                if (!success) {
                    debugWS.log(MOD_UPLOAD, LVL_WARN,
                        "[Uploader] Server accepted 0 readings. Response: " + response.substring(0, 200));
                }
            } else {
                success = responseDoc["success"].as<bool>();
                if (!success) {
                    debugWS.log(MOD_UPLOAD, LVL_WARN,
                        "[Uploader] Server returned success=false. Response: " + response.substring(0, 200));
                }
            }
        } else {
            debugWS.log(MOD_UPLOAD, LVL_WARN,
                "[Uploader] Server returned HTTP " + String(httpCode) + " (expected 200/202)");
        }
    } else {
        debugWS.log(MOD_UPLOAD, LVL_ERROR,
            "[Uploader] HTTP request failed: " + http.errorToString(httpCode));
    }

    http.end();
    return success;
}

// =============================================================================
// Account info — fetch from /api/radialog/verify and cache to LittleFS
// =============================================================================

bool Uploader::_fetchAccountInfo() {
    if (_config.radiamaps_url.length() == 0) return false;

    // Derive verify URL from configured upload URL by replacing trailing /upload with /verify
    String verifyUrl = _config.radiamaps_url;
    if (verifyUrl.endsWith("/upload")) {
        verifyUrl = verifyUrl.substring(0, verifyUrl.length() - 7) + "/verify";
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.begin(client, verifyUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _config.device_token);

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] Verifying token via " + verifyUrl);

    int httpCode = http.POST("");
    bool success = false;

    if (httpCode > 0) {
        String response = http.getString();
        debugWS.log(MOD_UPLOAD, LVL_INFO,
            "[Uploader] Verify HTTP " + String(httpCode) + ": " + response.substring(0, 300));

        if (httpCode == HTTP_CODE_OK) {
            JsonDocument responseDoc;
            DeserializationError err = deserializeJson(responseDoc, response);
            if (err != DeserializationError::Ok) {
                debugWS.log(MOD_UPLOAD, LVL_ERROR,
                    "[Uploader] Verify JSON parse error: " + String(err.c_str()));
            } else if (!responseDoc["username"].is<const char*>()) {
                debugWS.log(MOD_UPLOAD, LVL_WARN,
                    "[Uploader] Verify response missing 'username' field");
            }
            if (err == DeserializationError::Ok && responseDoc["username"].is<const char*>()) {
                _accountInfo.username          = responseDoc["username"].as<String>();
                _accountInfo.subscription      = responseDoc["subscription_status"].as<String>();

                // Try multiple field names the server might use for lifetime readings
                int64_t readings = -1;
                if (responseDoc.containsKey("total_readings")) {
                    readings = responseDoc["total_readings"].as<int64_t>();
                } else if (responseDoc.containsKey("lifetime_readings")) {
                    readings = responseDoc["lifetime_readings"].as<int64_t>();
                } else if (responseDoc.containsKey("readings_count")) {
                    readings = responseDoc["readings_count"].as<int64_t>();
                }
                _accountInfo.lifetime_readings = readings;

                _accountInfo.last_queried      = time(nullptr);
                _accountInfo.valid             = true;
                _lastAccountFetchMs            = millis();
                _saveAccountInfo();
                success = true;

                debugWS.log(MOD_UPLOAD, LVL_INFO,
                    "[Uploader] Account verified: " + _accountInfo.username +
                    " (" + _accountInfo.subscription +
                    ") lifetime=" + String((int32_t)_accountInfo.lifetime_readings));
            }
        } else {
            debugWS.log(MOD_UPLOAD, LVL_WARN,
                "[Uploader] Verify got HTTP " + String(httpCode) + " (expected 200)");
        }
    } else {
        debugWS.log(MOD_UPLOAD, LVL_WARN,
            "[Uploader] Verify request failed: " + http.errorToString(httpCode));
    }

    http.end();
    return success;
}

void Uploader::_saveAccountInfo() {
    JsonDocument doc;
    doc["username"]          = _accountInfo.username;
    doc["subscription"]      = _accountInfo.subscription;
    doc["lifetime_readings"] = _accountInfo.lifetime_readings;
    doc["last_queried"]      = static_cast<uint32_t>(_accountInfo.last_queried);

    File f = LittleFS.open(ACCOUNT_INFO_PATH, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

void Uploader::_loadAccountInfo() {
    File f = LittleFS.open(ACCOUNT_INFO_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    _accountInfo.username          = doc["username"].as<String>();
    _accountInfo.subscription      = doc["subscription"].as<String>();
    _accountInfo.lifetime_readings = doc["lifetime_readings"].as<int64_t>();
    _accountInfo.last_queried      = static_cast<time_t>(doc["last_queried"].as<uint32_t>());
    _accountInfo.valid             = _accountInfo.username.length() > 0;

    if (_accountInfo.valid) {
        debugWS.log(MOD_UPLOAD, LVL_INFO,
            "[Uploader] Loaded cached account: " + _accountInfo.username);
    }
}

// =============================================================================
// Persist last upload time to LittleFS
// =============================================================================

void Uploader::_saveLastUploadEpoch() {
    // Save to LittleFS (primary)
    JsonDocument doc;
    doc["epoch"] = static_cast<uint32_t>(_lastUploadEpoch);

    File f = LittleFS.open(LAST_UPLOAD_PATH, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }

    // Save to NVS (backup — survives LittleFS format)
    Preferences prefs;
    if (prefs.begin("radialog", false)) {
        prefs.putULong("last_upload", static_cast<uint32_t>(_lastUploadEpoch));
        prefs.end();
    }
}

void Uploader::_loadLastUploadEpoch() {
    // Try LittleFS first
    File f = LittleFS.open(LAST_UPLOAD_PATH, "r");
    if (f) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (!err) {
            _lastUploadEpoch = static_cast<time_t>(doc["epoch"].as<uint32_t>());
            return;
        }
    }

    // Fallback to NVS
    Preferences prefs;
    if (prefs.begin("radialog", true)) {
        uint32_t epoch = prefs.getULong("last_upload", 0);
        prefs.end();
        if (epoch > 0) {
            _lastUploadEpoch = static_cast<time_t>(epoch);
        }
    }
}

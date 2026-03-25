#include "uploader.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include "portal/debug_ws.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// =============================================================================
// RadiaLog Firmware - Upload Manager Implementation
// Uploads once per day at midnight UTC + random jitter (0-59 min).
// On WiFi connect, uploads immediately if >24h since last upload or if there
// is a backlog of pending readings. Drains all pending readings in batches.
// =============================================================================

static const char* LAST_UPLOAD_PATH = "/last_upload.json";

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
{
}

void Uploader::begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi) {
    _config = config;
    _buffer = buffer;
    _wifi   = wifi;

    // Random jitter: 0-59 minutes past midnight UTC for daily upload
    _jitterMinutes = static_cast<uint8_t>(esp_random() % 60);

    // Load last upload time from LittleFS
    _loadLastUploadEpoch();

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

bool Uploader::isUploading() const {
    return _uploading;
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

    // Never uploaded — upload now
    if (_lastUploadEpoch == 0) return true;

    // Check if we've crossed midnight UTC + jitter since last upload
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

        // Only attempt upload if WiFi STA is connected and upload is due
        if (self->_wifi != nullptr && self->_wifi->isSTAConnected()) {
            if (self->_isUploadDue()) {
                self->_forceFlag = false;
                self->_runUploadCycle();
            }
        }

        self->_forceFlag = false;
    }
}

// =============================================================================
// Upload Cycle — drain all pending readings in batches
// =============================================================================

void Uploader::_runUploadCycle() {
    if (_buffer == nullptr) return;

    BufferStats stats = _buffer->getStats();
    if (stats.pending == 0) {
        _uploadDueOnConnect = false;
        return;
    }

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] Starting upload cycle. Pending: " + String(stats.pending));

    Reading* batch = new Reading[MAX_BATCH_SIZE];
    if (batch == nullptr) return;

    uint32_t totalUploaded = 0;

    for (;;) {
        uint32_t count = _buffer->getUnuploaded(batch, MAX_BATCH_SIZE);
        if (count == 0) break;

        // Build JSON payload
        JsonDocument doc;
        JsonArray readings = doc["readings"].to<JsonArray>();

        uint32_t* ids = new uint32_t[count];
        if (ids == nullptr) break;

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

        String jsonPayload;
        serializeJson(doc, jsonPayload);

        _uploading = true;
        bool success = _postBatch(
            reinterpret_cast<const uint8_t*>(jsonPayload.c_str()),
            jsonPayload.length()
        );
        _uploading = false;

        if (success) {
            _buffer->markUploaded(ids, count);
            _lastUploadTime = millis();
            _backoffMs = BACKOFF_INITIAL_MS;
            totalUploaded += count;
            delete[] ids;
        } else {
            delete[] ids;
            vTaskDelay(pdMS_TO_TICKS(_backoffMs));
            if (_backoffMs < BACKOFF_MAX_MS) {
                _backoffMs *= 2;
                if (_backoffMs > BACKOFF_MAX_MS) _backoffMs = BACKOFF_MAX_MS;
            }
            delete[] batch;
            debugWS.log(MOD_UPLOAD, LVL_WARN,
                "[Uploader] Batch failed after " + String(totalUploaded) + " readings. Will retry.");
            return;
        }
    }

    delete[] batch;

    // Record successful upload time
    _lastUploadEpoch = time(nullptr);
    _saveLastUploadEpoch();
    _uploadDueOnConnect = false;

    debugWS.log(MOD_UPLOAD, LVL_INFO,
        "[Uploader] Upload complete. " + String(totalUploaded) + " readings uploaded.");
}

// =============================================================================
// HTTP POST
// =============================================================================

bool Uploader::_postBatch(const uint8_t* jsonPayload, size_t len) {
    if (_config.radiamaps_url.length() == 0) return false;

    HTTPClient http;
    http.begin(_config.radiamaps_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _config.device_token);
    http.addHeader("X-Device-Id", _config.device_id);

    int httpCode = http.POST(const_cast<uint8_t*>(jsonPayload), len);
    bool success = false;

    if (httpCode == HTTP_CODE_OK || httpCode == 202) {
        String response = http.getString();
        JsonDocument responseDoc;
        DeserializationError err = deserializeJson(responseDoc, response);
        if (err == DeserializationError::Ok) {
            // Server returns { accepted: N, errors: N } on 202
            // or { success: true } on 200
            if (responseDoc["accepted"].is<int>()) {
                success = responseDoc["accepted"].as<int>() > 0;
            } else {
                success = responseDoc["success"].as<bool>();
            }
        }
    }

    http.end();
    return success;
}

// =============================================================================
// Persist last upload time to LittleFS
// =============================================================================

void Uploader::_saveLastUploadEpoch() {
    JsonDocument doc;
    doc["epoch"] = static_cast<uint32_t>(_lastUploadEpoch);

    File f = LittleFS.open(LAST_UPLOAD_PATH, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

void Uploader::_loadLastUploadEpoch() {
    File f = LittleFS.open(LAST_UPLOAD_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    _lastUploadEpoch = static_cast<time_t>(doc["epoch"].as<uint32_t>());
}

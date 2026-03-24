#include "uploader.h"
#include "buffer.h"
#include "wifi_mgr.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =============================================================================
// RadiaLog Firmware - Upload Manager Implementation
// Runs as a FreeRTOS task on core 0. Batch uploads unuploaded readings
// to RadiaMaps via HTTPS with exponential backoff on failure.
// =============================================================================

Uploader::Uploader()
    : _buffer(nullptr)
    , _wifi(nullptr)
    , _uploading(false)
    , _lastUploadTime(0)
    , _forceFlag(false)
    , _taskHandle(nullptr)
    , _backoffMs(BACKOFF_INITIAL_MS)
{
}

void Uploader::begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi) {
    _config = config;
    _buffer = buffer;
    _wifi   = wifi;

    // Create the upload task pinned to core 0
    xTaskCreatePinnedToCore(
        _uploadTask,        // Task function
        "uploader",         // Task name
        TASK_STACK_SIZE,    // Stack size (bytes)
        this,               // Parameter (pointer to this Uploader)
        TASK_PRIORITY,      // Priority
        &_taskHandle,       // Task handle
        0                   // Core 0
    );
}

void Uploader::forceUpload() {
    _forceFlag = true;
    // If the task is waiting, notify it to wake up immediately
    if (_taskHandle != nullptr) {
        xTaskNotifyGive(_taskHandle);
    }
}

unsigned long Uploader::getLastUploadTime() const {
    return _lastUploadTime;
}

bool Uploader::isUploading() const {
    return _uploading;
}

// =============================================================================
// FreeRTOS Task
// =============================================================================

void Uploader::_uploadTask(void* pvParameters) {
    Uploader* self = static_cast<Uploader*>(pvParameters);

    for (;;) {
        // Wait for the upload cycle interval, or wake on forceUpload() notification
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(UPLOAD_CYCLE_MS));

        // Clear force flag
        self->_forceFlag = false;

        // Only attempt upload if WiFi STA is connected
        if (self->_wifi != nullptr && self->_wifi->isSTAConnected()) {
            self->_runUploadCycle();
        }
    }
}

// =============================================================================
// Upload Cycle — drain all unuploaded readings in batches
// =============================================================================

void Uploader::_runUploadCycle() {
    if (_buffer == nullptr) return;

    // Temporary buffer for batch of readings
    Reading* batch = new Reading[MAX_BATCH_SIZE];
    if (batch == nullptr) return;

    bool hadSuccess = false;

    for (;;) {
        // Fetch up to MAX_BATCH_SIZE unuploaded readings
        uint32_t count = _buffer->getUnuploaded(batch, MAX_BATCH_SIZE);
        if (count == 0) {
            break; // All caught up
        }

        // Build JSON payload using ArduinoJson
        // Each reading needs ~200 bytes of JSON; use dynamic allocation
        JsonDocument doc;
        JsonArray readings = doc["readings"].to<JsonArray>();

        // Collect IDs for marking uploaded on success
        uint32_t* ids = new uint32_t[count];
        if (ids == nullptr) {
            break;
        }

        for (uint32_t i = 0; i < count; i++) {
            JsonObject obj = readings.add<JsonObject>();
            obj["id"]               = batch[i].id;
            obj["timestamp"]        = batch[i].timestamp;
            obj["lat"]              = batch[i].lat;
            obj["lon"]              = batch[i].lon;
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

        // Serialize JSON to string
        String jsonPayload;
        serializeJson(doc, jsonPayload);

        // POST the batch
        _uploading = true;
        bool success = _postBatch(
            reinterpret_cast<const uint8_t*>(jsonPayload.c_str()),
            jsonPayload.length()
        );
        _uploading = false;

        if (success) {
            // Mark readings as uploaded
            _buffer->markUploaded(ids, count);
            _lastUploadTime = millis();
            _backoffMs = BACKOFF_INITIAL_MS; // Reset backoff on success
            hadSuccess = true;
        } else {
            // Backoff on failure — stop this cycle and wait
            delete[] ids;
            vTaskDelay(pdMS_TO_TICKS(_backoffMs));

            // Exponential backoff: double the delay, cap at max
            if (_backoffMs < BACKOFF_MAX_MS) {
                _backoffMs *= 2;
                if (_backoffMs > BACKOFF_MAX_MS) {
                    _backoffMs = BACKOFF_MAX_MS;
                }
            }
            delete[] batch;
            return; // Exit cycle; will retry next time
        }

        delete[] ids;
    }

    delete[] batch;
}

// =============================================================================
// HTTP POST — send a batch payload to RadiaMaps
// =============================================================================

bool Uploader::_postBatch(const uint8_t* jsonPayload, size_t len) {
    if (_config.radiamaps_url.length() == 0) {
        return false;
    }

    HTTPClient http;
    http.begin(_config.radiamaps_url);

    // Set required headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _config.device_token);
    http.addHeader("X-Device-Id", _config.device_id);

    // POST the JSON payload
    int httpCode = http.POST(const_cast<uint8_t*>(jsonPayload), len);

    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        // Parse response to check for success:true
        String response = http.getString();
        JsonDocument responseDoc;
        DeserializationError err = deserializeJson(responseDoc, response);
        if (err == DeserializationError::Ok) {
            success = responseDoc["success"].as<bool>();
        }
    }

    http.end();
    return success;
}

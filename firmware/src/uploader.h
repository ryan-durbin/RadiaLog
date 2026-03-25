#pragma once

#include <Arduino.h>

// =============================================================================
// RadiaLog Firmware - Upload Manager
// Runs as a FreeRTOS task on core 0. Uploads once per day (midnight UTC +
// random jitter) or immediately on WiFi connect if >24h since last upload.
// Batches readings in chunks to avoid heap exhaustion.
// =============================================================================

class ReadingBuffer;
class WifiMgr;

/// Configuration for the upload endpoint.
struct UploadConfig {
    String radiamaps_url;    ///< Full URL for the upload endpoint
    String device_token;     ///< Authentication token (X-Device-Token header)
    String device_id;        ///< Device identifier (X-Device-Id header)
};

class Uploader {
public:
    Uploader();

    /// Start the uploader FreeRTOS task (pinned to core 0).
    void begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi);

    /// Signal the upload task to wake and check if an upload is due.
    void forceUpload();

    /// Call when WiFi connects — triggers upload if overdue.
    void onWifiConnect();

    /// Returns millis() timestamp of the last successful upload, or 0 if none.
    unsigned long getLastUploadTime() const;

    /// Returns true if an upload HTTP request is currently in progress.
    bool isUploading() const;

private:
    UploadConfig   _config;
    ReadingBuffer* _buffer;
    WifiMgr*       _wifi;

    volatile bool          _uploading;
    volatile unsigned long _lastUploadTime;
    volatile bool          _forceFlag;
    TaskHandle_t           _taskHandle;

    // Exponential backoff state
    unsigned long _backoffMs;
    static constexpr unsigned long BACKOFF_INITIAL_MS = 1000UL;
    static constexpr unsigned long BACKOFF_MAX_MS     = 300000UL; // 5 minutes

    // Upload batch — chunked to stay within heap limits.
    // Each reading ≈ 200 bytes JSON; 500 readings ≈ 100KB payload.
    static constexpr uint32_t MAX_BATCH_SIZE = 500;

    // Task checks every 60s whether an upload is due.
    static constexpr unsigned long CHECK_INTERVAL_MS = 60000UL;

    static constexpr uint32_t TASK_STACK_SIZE = 12288;
    static constexpr UBaseType_t TASK_PRIORITY = 1;

    // Scheduling state
    time_t   _lastUploadEpoch;     // Unix time of last successful upload (persisted)
    uint8_t  _jitterMinutes;       // Random 0-59 minute offset for daily upload
    bool     _uploadDueOnConnect;  // Set by onWifiConnect if overdue

    /// Check if an upload should happen now.
    bool _isUploadDue() const;

    /// Persist/load the last upload timestamp so it survives reboots.
    void _saveLastUploadEpoch();
    void _loadLastUploadEpoch();

    static void _uploadTask(void* pvParameters);
    void _runUploadCycle();
    bool _postBatch(const uint8_t* jsonPayload, size_t len);
};

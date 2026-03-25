#pragma once

#include <Arduino.h>

// =============================================================================
// RadiaLog Firmware - Upload Manager
// Runs as a FreeRTOS task on core 0, uploads unsent readings via HTTPS.
// Batches up to 10,000 readings per request with exponential backoff.
// =============================================================================

// Forward declarations to avoid circular dependencies
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
    /// @param config  Upload endpoint configuration
    /// @param buffer  Pointer to the ReadingBuffer for fetching unuploaded readings
    /// @param wifi    Pointer to WifiMgr for checking STA connection status
    void begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi);

    /// Signal the upload task to wake up and attempt upload immediately.
    void forceUpload();

    /// Returns millis() timestamp of the last successful upload, or 0 if none.
    unsigned long getLastUploadTime() const;

    /// Returns true if an upload HTTP request is currently in progress.
    bool isUploading() const;

private:
    // Upload endpoint config
    UploadConfig _config;

    // Dependencies (non-owning pointers)
    ReadingBuffer* _buffer;
    WifiMgr*       _wifi;

    // State
    volatile bool          _uploading;
    volatile unsigned long _lastUploadTime;
    volatile bool          _forceFlag;

    // FreeRTOS task handle
    TaskHandle_t _taskHandle;

    // Exponential backoff state
    unsigned long _backoffMs;
    static constexpr unsigned long BACKOFF_INITIAL_MS = 1000UL;
    static constexpr unsigned long BACKOFF_MAX_MS     = 300000UL; // 5 minutes

    // Upload batch size — keep small to avoid heap exhaustion.
    // Each Reading is ~48 bytes; 50 readings ≈ 2.4KB heap + ~10KB JSON.
    static constexpr uint32_t MAX_BATCH_SIZE = 50;

    // Task timing
    static constexpr unsigned long UPLOAD_CYCLE_MS = 30000UL; // 30s between cycles

    // Task stack and priority — needs room for HTTP + JSON serialization
    static constexpr uint32_t TASK_STACK_SIZE = 12288;
    static constexpr UBaseType_t TASK_PRIORITY = 1;

    /// FreeRTOS task entry point (static).
    static void _uploadTask(void* pvParameters);

    /// Run one upload cycle: fetch batch, POST, mark uploaded. Repeat until drained.
    void _runUploadCycle();

    /// POST a batch of readings. Returns true on success.
    bool _postBatch(const uint8_t* jsonPayload, size_t len);
};

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

/// Cached RadiaMaps account info from /api/radialog/verify.
struct AccountInfo {
    String username;
    String subscription;
    int64_t lifetime_readings;
    bool    valid;              ///< True if we have successfully fetched account info
    time_t  last_queried;       ///< Unix epoch of last successful server query

    AccountInfo() : lifetime_readings(-1), valid(false), last_queried(0) {}
};

class Uploader {
public:
    Uploader();

    /// Start the uploader FreeRTOS task (pinned to core 0).
    void begin(const UploadConfig& config, ReadingBuffer* buffer, WifiMgr* wifi);

    /// Update upload config (e.g. after token change in settings).
    void updateConfig(const UploadConfig& config);

    /// Signal the upload task to wake and check if an upload is due.
    void forceUpload();

    /// Call when WiFi connects — triggers upload if overdue.
    void onWifiConnect();

    /// Returns millis() timestamp of the last successful upload, or 0 if none.
    unsigned long getLastUploadTime() const;

    /// Returns Unix epoch of the last successful upload, or 0 if none (persisted).
    time_t getLastUploadEpoch() const;

    /// Returns true if an upload HTTP request is currently in progress.
    bool isUploading() const;

    /// Returns cached RadiaMaps account info.
    const AccountInfo& getAccountInfo() const;

    /// Trigger an immediate account info refresh (e.g. after token change).
    void refreshAccountInfo();

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

    // Upload batch — sized to leave enough heap for SSL/TLS buffers (~50KB).
    // Each reading ≈ 182 bytes total heap (48 struct + 4 id + 130 JSON).
    // 250 readings ≈ 32KB payload, ~45KB heap.  JsonDocument freed before
    // SSL allocation so peak heap ≈ payload + SSL ≈ 82KB (within ~300KB free).
    static constexpr uint32_t MAX_BATCH_SIZE = 250;

    // Task checks every 60s whether an upload is due.
    static constexpr unsigned long CHECK_INTERVAL_MS = 60000UL;

    static constexpr uint32_t TASK_STACK_SIZE = 16384;
    static constexpr UBaseType_t TASK_PRIORITY = 1;

    // Scheduling state
    time_t   _lastUploadEpoch;     // Unix time of last successful upload (persisted)
    uint8_t  _jitterMinutes;       // Random 0-59 minute offset for daily upload
    bool     _uploadDueOnConnect;  // Set by onWifiConnect if overdue
    unsigned long _serverUnreachableUntilMs;  // millis() timestamp; skip uploads until this time

    // Account info cache
    AccountInfo    _accountInfo;
    unsigned long  _lastAccountFetchMs;    // millis() of last successful fetch
    volatile bool  _accountRefreshFlag;    // Request immediate refresh
    static constexpr unsigned long ACCOUNT_REFRESH_INTERVAL_MS = 3600000UL; // 1 hour

    /// Check if an upload should happen now.
    bool _isUploadDue() const;

    /// Persist/load the last upload timestamp so it survives reboots.
    void _saveLastUploadEpoch();
    void _loadLastUploadEpoch();

    /// Fetch account info from /api/radialog/verify and cache to LittleFS.
    bool _fetchAccountInfo();
    void _saveAccountInfo();
    void _loadAccountInfo();

    /// Lightweight server reachability check (HEAD request).
    bool _pingServer();

    static constexpr unsigned long SERVER_RETRY_DELAY_MS = 3600000UL; // 1 hour

    static void _uploadTask(void* pvParameters);
    void _runUploadCycle();
    bool _postBatch(const uint8_t* jsonPayload, size_t len);
};

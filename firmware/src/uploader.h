#pragma once
#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>
#include "config_mgr.h"
#include "buffer.h"

// =============================================================================
// RadiaLog Firmware - Upload Manager
// Runs as a FreeRTOS task on core 0, uploads unsent readings via HTTP.
// =============================================================================

class Uploader {
public:
    Uploader();

    /// Start the uploader FreeRTOS task (pinned to core 0).
    void begin(ConfigMgr& cfg, ReadingBuffer& buf);

    /// Returns true if an upload is currently in progress.
    bool isUploading() const;

private:
    bool         _uploading;
    ConfigMgr*   _cfg;
    ReadingBuffer* _buf;

    static void _uploadTask(void* pvParameters);
};

#endif // UPLOADER_H

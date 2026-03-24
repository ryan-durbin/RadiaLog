#include "uploader.h"

// =============================================================================
// RadiaLog Firmware - Uploader Stub Implementation
// Real implementation uses xTaskCreatePinnedToCore + HTTPClient for upload.
// =============================================================================

Uploader::Uploader()
    : _uploading(false)
    , _cfg(nullptr)
    , _buf(nullptr)
{
}

void Uploader::begin(ConfigMgr& cfg, ReadingBuffer& buf) {
    _cfg = &cfg;
    _buf = &buf;
    // Stub: skip FreeRTOS task creation
    // Real implementation:
    //   xTaskCreatePinnedToCore(_uploadTask, "uploader", 8192, this, 1, nullptr, 0);
}

bool Uploader::isUploading() const {
    return _uploading;
}

void Uploader::_uploadTask(void* pvParameters) {
    // Stub: task body — real implementation polls buffer and uploads via HTTP
    (void)pvParameters;
    vTaskDelete(nullptr);
}

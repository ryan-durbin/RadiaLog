#include "buffer.h"

// =============================================================================
// RadiaLog Firmware - ReadingBuffer Stub Implementation
// Real implementation uses LittleFS for persistent reading storage.
// This stub compiles and returns safe defaults.
// =============================================================================

ReadingBuffer::ReadingBuffer()
    : _count(0)
    , _unsentCount(0)
{
}

void ReadingBuffer::begin() {
    // Stub: no LittleFS initialization
    // Real implementation: mount LittleFS, read index file
    _count = 0;
    _unsentCount = 0;
}

bool ReadingBuffer::append(const Reading& r) {
    // Stub: do nothing, return false (not stored)
    (void)r;
    return false;
}

uint32_t ReadingBuffer::count() const {
    return _count;
}

uint32_t ReadingBuffer::unsentCount() const {
    return _unsentCount;
}

void ReadingBuffer::markSent(uint32_t count) {
    // Stub: do nothing
    (void)count;
}

uint32_t ReadingBuffer::getUnsent(Reading* buf, uint32_t max) {
    // Stub: return 0 readings
    (void)buf;
    (void)max;
    return 0;
}

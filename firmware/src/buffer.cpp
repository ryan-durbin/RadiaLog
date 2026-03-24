#include "buffer.h"

// =============================================================================
// RadiaLog Firmware - ReadingBuffer Implementation
// LittleFS-backed persistent reading storage.
// US-002: Skeleton with BufferStats and getStats(); other methods are stubs.
// =============================================================================

ReadingBuffer::ReadingBuffer()
    : _depth(0)
    , _lifetimeLogged(0)
    , _lifetimeUploaded(0)
{
}

bool ReadingBuffer::begin() {
    // Initialize counters.
    // Full implementation: mount LittleFS, load index file, restore counters.
    _depth = 0;
    _lifetimeLogged = 0;
    _lifetimeUploaded = 0;
    return true;
}

BufferStats ReadingBuffer::getStats() const {
    BufferStats stats;
    stats.depth            = _depth;
    stats.lifetimeLogged   = _lifetimeLogged;
    stats.lifetimeUploaded = _lifetimeUploaded;
    return stats;
}

bool ReadingBuffer::appendReading(const Reading& r) {
    // Stub: LittleFS write not yet implemented.
    (void)r;
    return false;
}

uint32_t ReadingBuffer::getUnuploaded(Reading* buf, uint32_t count) {
    // Stub: no stored readings yet.
    (void)buf;
    (void)count;
    return 0;
}

void ReadingBuffer::markUploaded(const uint32_t* ids, uint32_t count) {
    // Stub: no stored readings yet.
    (void)ids;
    (void)count;
}

void ReadingBuffer::pruneUploaded() {
    // Stub: no stored readings yet.
    // Full implementation: scan storage and remove uploaded == true records.
}

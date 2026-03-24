#include "buffer.h"
#include <LittleFS.h>

// =============================================================================
// RadiaLog Firmware - ReadingBuffer Implementation
// LittleFS-backed persistent reading storage.
// US-003: LittleFS init and index file for ReadingBuffer.
// =============================================================================

// File paths on LittleFS
static const char* READINGS_FILE = "/readings.bin";
static const char* INDEX_FILE    = "/readings_idx.bin";

// Index file layout: uint32_t[3] = { depth, lifetimeLogged, lifetimeUploaded }
static const size_t INDEX_SIZE = sizeof(uint32_t) * 3;

ReadingBuffer::ReadingBuffer()
    : _depth(0)
    , _lifetimeLogged(0)
    , _lifetimeUploaded(0)
{
}

bool ReadingBuffer::begin() {
    // Mount LittleFS, format on first use if needed.
    if (!LittleFS.begin(true)) {
        return false;
    }

    // Create /readings.bin if it does not exist.
    if (!LittleFS.exists(READINGS_FILE)) {
        File f = LittleFS.open(READINGS_FILE, "w");
        if (!f) {
            return false;
        }
        f.close();
    }

    // Load or initialize stats from /readings_idx.bin.
    if (!LittleFS.exists(INDEX_FILE)) {
        // Index absent: initialize counters to zero.
        _depth            = 0;
        _lifetimeLogged   = 0;
        _lifetimeUploaded = 0;
    } else {
        // Index present: try to read uint32_t[3].
        File idx = LittleFS.open(INDEX_FILE, "r");
        if (!idx) {
            // Cannot open — fall back to zeros.
            _depth            = 0;
            _lifetimeLogged   = 0;
            _lifetimeUploaded = 0;
        } else {
            uint32_t buf[3] = {0, 0, 0};
            size_t bytesRead = idx.read(reinterpret_cast<uint8_t*>(buf), INDEX_SIZE);
            idx.close();

            if (bytesRead < INDEX_SIZE) {
                // Corrupt / undersized — fall back to zeros.
                _depth            = 0;
                _lifetimeLogged   = 0;
                _lifetimeUploaded = 0;
            } else {
                _depth            = buf[0];
                _lifetimeLogged   = buf[1];
                _lifetimeUploaded = buf[2];
            }
        }
    }

    // Write-through: persist the (possibly freshly initialized) index.
    return _saveIndex();
}

bool ReadingBuffer::_saveIndex() {
    File idx = LittleFS.open(INDEX_FILE, "w");
    if (!idx) {
        return false;
    }
    uint32_t buf[3] = { _depth, _lifetimeLogged, _lifetimeUploaded };
    size_t written = idx.write(reinterpret_cast<const uint8_t*>(buf), INDEX_SIZE);
    idx.close();
    return written == INDEX_SIZE;
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

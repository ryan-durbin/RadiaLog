#include "buffer.h"
#include <LittleFS.h>

// =============================================================================
// RadiaLog Firmware - ReadingBuffer Implementation
// LittleFS-backed persistent reading storage.
// US-003: LittleFS init and index file for ReadingBuffer.
// =============================================================================

// File paths on LittleFS
static const char* READINGS_FILE = "/readings.bin";
static const char* STATUS_FILE   = "/readings_status.bin";
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
    // Open /readings.bin in append mode.
    File f = LittleFS.open(READINGS_FILE, "a");
    if (!f) {
        return false;
    }

    // Serialize the reading into a 34-byte binary record.
    // Layout: float lat(4) + float lon(4) + float dose_rate(4)
    //   + float count_rate(4) + uint32_t timestamp(4) + float accuracy(4)
    //   + float altitude(4) + uint16_t speed_mph_x10(2)
    //   + uint16_t speed_kph_x10(2) + uint16_t heading_x10(2) = 34 bytes
    uint8_t record[READING_BINARY_SIZE];
    size_t offset = 0;

    // Helper: write a float (4 bytes) into record at offset.
    auto writeFloat = [&](float val) {
        memcpy(&record[offset], &val, sizeof(float));
        offset += sizeof(float);
    };
    // Helper: write a uint32_t (4 bytes) into record at offset.
    auto writeU32 = [&](uint32_t val) {
        memcpy(&record[offset], &val, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    };
    // Helper: write a uint16_t (2 bytes) into record at offset.
    auto writeU16 = [&](uint16_t val) {
        memcpy(&record[offset], &val, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    };

    writeFloat(r.lat);
    writeFloat(r.lon);
    writeFloat(r.dose_rate);
    writeFloat(r.count_rate);
    writeU32(r.timestamp);
    writeFloat(r.accuracy);
    writeFloat(r.altitude);
    writeU16(static_cast<uint16_t>(r.speed_mph * 10.0f));
    writeU16(static_cast<uint16_t>(r.speed_kph * 10.0f));
    writeU16(static_cast<uint16_t>(r.heading * 10.0f));

    // Write exactly READING_BINARY_SIZE bytes.
    size_t written = f.write(record, READING_BINARY_SIZE);
    f.close();
    if (written != READING_BINARY_SIZE) {
        return false;
    }

    // Append a status byte (0 = pending) to /readings_status.bin.
    File sf = LittleFS.open(STATUS_FILE, "a");
    if (!sf) {
        return false;
    }
    uint8_t pending = 0;
    size_t sw = sf.write(&pending, 1);
    sf.close();
    if (sw != 1) {
        return false;
    }

    // Update in-memory counters and persist index.
    _depth++;
    _lifetimeLogged++;

    return _saveIndex();
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

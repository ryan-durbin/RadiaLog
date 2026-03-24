#include "buffer.h"
#include <LittleFS.h>

// =============================================================================
// RadiaLog Firmware - ReadingBuffer Implementation
// LittleFS-backed persistent reading storage with upload tracking.
// =============================================================================

// File paths on LittleFS
static const char* READINGS_FILE = "/readings.bin";
static const char* STATUS_FILE   = "/readings_status.bin";
static const char* INDEX_FILE    = "/readings_idx.bin";

// Index file layout: uint32_t[3] = { depth, lifetimeLogged, lifetimeUploaded }
static const size_t INDEX_SIZE = sizeof(uint32_t) * 3;

// Prune when buffer depth exceeds this count (~48K readings ≈ 1.6MB)
static const uint32_t PRUNE_THRESHOLD = 48000;

ReadingBuffer::ReadingBuffer()
    : _depth(0)
    , _lifetimeLogged(0)
    , _lifetimeUploaded(0)
    , _uploadedInBuffer(0)
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

    // Create /readings_status.bin if it does not exist.
    if (!LittleFS.exists(STATUS_FILE)) {
        File f = LittleFS.open(STATUS_FILE, "w");
        if (!f) {
            return false;
        }
        f.close();
    }

    // Load or initialize stats from /readings_idx.bin.
    if (!LittleFS.exists(INDEX_FILE)) {
        _depth            = 0;
        _lifetimeLogged   = 0;
        _lifetimeUploaded = 0;
    } else {
        File idx = LittleFS.open(INDEX_FILE, "r");
        if (!idx) {
            _depth            = 0;
            _lifetimeLogged   = 0;
            _lifetimeUploaded = 0;
        } else {
            uint32_t buf[3] = {0, 0, 0};
            size_t bytesRead = idx.read(reinterpret_cast<uint8_t*>(buf), INDEX_SIZE);
            idx.close();

            if (bytesRead < INDEX_SIZE) {
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

    // Count currently-uploaded readings in the status file
    _uploadedInBuffer = 0;
    if (LittleFS.exists(STATUS_FILE) && _depth > 0) {
        File sf = LittleFS.open(STATUS_FILE, "r");
        if (sf) {
            while (sf.available()) {
                uint8_t status;
                if (sf.read(&status, 1) != 1) break;
                if (status == 1) _uploadedInBuffer++;
            }
            sf.close();
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
    stats.pending          = _depth - _uploadedInBuffer;
    stats.lifetimeLogged   = _lifetimeLogged;
    stats.lifetimeUploaded = _lifetimeUploaded;
    return stats;
}

// =============================================================================
// appendReading — serialize and write one reading to flash
// =============================================================================

bool ReadingBuffer::appendReading(const Reading& r) {
    File f = LittleFS.open(READINGS_FILE, "a");
    if (!f) {
        return false;
    }

    // Serialize into 34-byte binary record.
    uint8_t record[READING_BINARY_SIZE];
    size_t offset = 0;

    auto writeFloat = [&](float val) {
        memcpy(&record[offset], &val, sizeof(float));
        offset += sizeof(float);
    };
    auto writeU32 = [&](uint32_t val) {
        memcpy(&record[offset], &val, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    };
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

    size_t written = f.write(record, READING_BINARY_SIZE);
    f.close();
    if (written != READING_BINARY_SIZE) {
        return false;
    }

    // Append a status byte (0 = pending) to status file.
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

    _depth++;
    _lifetimeLogged++;

    return _saveIndex();
}

// =============================================================================
// _deserializeReading — convert 34 binary bytes back into a Reading struct
// =============================================================================

static void _deserializeReading(const uint8_t* record, Reading& r) {
    size_t offset = 0;

    auto readFloat = [&]() -> float {
        float val;
        memcpy(&val, &record[offset], sizeof(float));
        offset += sizeof(float);
        return val;
    };
    auto readU32 = [&]() -> uint32_t {
        uint32_t val;
        memcpy(&val, &record[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);
        return val;
    };
    auto readU16 = [&]() -> uint16_t {
        uint16_t val;
        memcpy(&val, &record[offset], sizeof(uint16_t));
        offset += sizeof(uint16_t);
        return val;
    };

    r.lat        = readFloat();
    r.lon        = readFloat();
    r.dose_rate  = readFloat();
    r.count_rate = readFloat();
    r.timestamp  = readU32();
    r.accuracy   = readFloat();
    r.altitude   = readFloat();
    r.speed_mph  = readU16() / 10.0f;
    r.speed_kph  = readU16() / 10.0f;
    r.heading    = readU16() / 10.0f;

    r.altitude_accuracy = 0.0f;
    r.gps_valid = (r.lat != 0.0f || r.lon != 0.0f);
}

// =============================================================================
// getUnuploaded — retrieve pending readings from flash (oldest first)
// =============================================================================

uint32_t ReadingBuffer::getUnuploaded(Reading* buf, uint32_t count) {
    if (_depth == 0 || count == 0) return 0;

    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) return 0;

    File rf = LittleFS.open(READINGS_FILE, "r");
    if (!rf) { sf.close(); return 0; }

    uint32_t found = 0;
    uint32_t index = 0;

    while (found < count && sf.available()) {
        uint8_t status;
        if (sf.read(&status, 1) != 1) break;

        if (status == 0) {
            // This reading is pending upload — deserialize it
            if (!rf.seek(static_cast<size_t>(index) * READING_BINARY_SIZE)) break;

            uint8_t record[READING_BINARY_SIZE];
            if (rf.read(record, READING_BINARY_SIZE) != READING_BINARY_SIZE) break;

            Reading& r = buf[found];
            _deserializeReading(record, r);
            r.id       = index;
            r.uploaded = false;

            found++;
        }
        index++;
    }

    sf.close();
    rf.close();
    return found;
}

// =============================================================================
// markUploaded — set status bytes to 1 for the given reading IDs
// =============================================================================

void ReadingBuffer::markUploaded(const uint32_t* ids, uint32_t count) {
    if (count == 0 || _depth == 0) return;

    // Read entire status file into memory
    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) return;

    size_t statusSize = sf.size();
    if (statusSize == 0) { sf.close(); return; }

    uint8_t* statusBuf = new (std::nothrow) uint8_t[statusSize];
    if (statusBuf == nullptr) { sf.close(); return; }

    sf.read(statusBuf, statusSize);
    sf.close();

    // Mark requested IDs as uploaded
    uint32_t marked = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = ids[i];
        if (id < statusSize && statusBuf[id] == 0) {
            statusBuf[id] = 1;
            marked++;
        }
    }

    if (marked > 0) {
        // Write modified status file back
        File wf = LittleFS.open(STATUS_FILE, "w");
        if (wf) {
            wf.write(statusBuf, statusSize);
            wf.close();
        }

        _lifetimeUploaded += marked;
        _uploadedInBuffer += marked;
        _saveIndex();
    }

    delete[] statusBuf;
}

// =============================================================================
// pruneUploaded — compact storage by removing leading uploaded readings
// =============================================================================

void ReadingBuffer::pruneUploaded() {
    if (_depth < PRUNE_THRESHOLD) return;

    // Read status file to find contiguous uploaded block at the start
    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) return;

    size_t statusSize = sf.size();
    if (statusSize == 0) { sf.close(); return; }

    // Count contiguous uploaded readings from the beginning
    uint32_t pruneCount = 0;
    while (pruneCount < statusSize) {
        uint8_t status;
        if (sf.read(&status, 1) != 1) break;
        if (status != 1) break;
        pruneCount++;
    }
    sf.close();

    if (pruneCount == 0) return;

    // --- Compact readings.bin: copy remaining records to temp file ---
    size_t skipBytes   = static_cast<size_t>(pruneCount) * READING_BINARY_SIZE;
    size_t remainCount = _depth - pruneCount;

    {
        File src = LittleFS.open(READINGS_FILE, "r");
        File dst = LittleFS.open("/readings_tmp.bin", "w");
        if (!src || !dst) { src.close(); dst.close(); return; }

        src.seek(skipBytes);
        uint8_t chunk[512];
        size_t remaining = remainCount * READING_BINARY_SIZE;
        while (remaining > 0) {
            size_t toRead = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);
            size_t got = src.read(chunk, toRead);
            if (got == 0) break;
            dst.write(chunk, got);
            remaining -= got;
        }
        src.close();
        dst.close();
    }

    // --- Compact status file: copy remaining bytes to temp file ---
    {
        File src = LittleFS.open(STATUS_FILE, "r");
        File dst = LittleFS.open("/status_tmp.bin", "w");
        if (!src || !dst) { src.close(); dst.close(); return; }

        src.seek(pruneCount);
        uint8_t chunk[512];
        size_t remaining = statusSize - pruneCount;
        while (remaining > 0) {
            size_t toRead = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);
            size_t got = src.read(chunk, toRead);
            if (got == 0) break;
            dst.write(chunk, got);
            remaining -= got;
        }
        src.close();
        dst.close();
    }

    // --- Swap files ---
    LittleFS.remove(READINGS_FILE);
    LittleFS.rename("/readings_tmp.bin", READINGS_FILE);
    LittleFS.remove(STATUS_FILE);
    LittleFS.rename("/status_tmp.bin", STATUS_FILE);

    // Update counters — pruned readings were all uploaded
    _depth -= pruneCount;
    _uploadedInBuffer -= pruneCount;
    _saveIndex();
}

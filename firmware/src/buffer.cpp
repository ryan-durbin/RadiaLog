#include "buffer.h"
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
    , _mutex(nullptr)
{
}

bool ReadingBuffer::begin() {
    
    if (!LittleFS.begin(false)) {                                                                                                       
           Serial.println("[BUFFER] LittleFS mount failed - likely first boot or storage issue");                                          
           // Try to format once (first boot only)                                                                                         
           LittleFS.format();                                                                                                              
           if (!LittleFS.begin(false)) {                                                                                                   
               Serial.println("[BUFFER] FATAL: Cannot mount LittleFS");                                                                    
               return false;                                                                                                               
           }                                                                                                                               
           Serial.println("[BUFFER] LittleFS initialized (new or reformatted).");                                                          
       } else {                                                                                                                            
           Serial.println("[BUFFER] LittleFS mounted successfully.");                                                                      
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

    // Reconcile _depth with actual file sizes (they can diverge after a crash
    // or index corruption). Use the minimum of status entries and complete
    // reading records as the authoritative depth.
    _uploadedInBuffer = 0;
    {
        uint32_t statusDepth = 0;
        uint32_t readingsDepth = 0;

        if (LittleFS.exists(STATUS_FILE)) {
            File sf = LittleFS.open(STATUS_FILE, "r");
            if (sf) { statusDepth = sf.size(); sf.close(); }
        }
        if (LittleFS.exists(READINGS_FILE)) {
            File rf = LittleFS.open(READINGS_FILE, "r");
            if (rf) { readingsDepth = rf.size() / READING_BINARY_SIZE; rf.close(); }
        }

        // Use the minimum — a partial write may have updated one file but not the other.
        uint32_t actualDepth = (statusDepth < readingsDepth) ? statusDepth : readingsDepth;
        if (actualDepth != _depth) {
            _depth = actualDepth;
        }

        // Count uploaded entries within the reconciled depth.
        if (_depth > 0 && LittleFS.exists(STATUS_FILE)) {
            File sf = LittleFS.open(STATUS_FILE, "r");
            if (sf) {
                for (uint32_t i = 0; i < _depth && sf.available(); i++) {
                    uint8_t status;
                    if (sf.read(&status, 1) != 1) break;
                    if (status == 1) _uploadedInBuffer++;
                }
                sf.close();
            }
        }
    }

    // Create the FreeRTOS mutex for thread-safe file I/O.
    _mutex = xSemaphoreCreateMutex();
    assert(_mutex != nullptr);

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
    stats.pending          = (_uploadedInBuffer >= _depth) ? 0 : _depth - _uploadedInBuffer;
    stats.lifetimeLogged   = _lifetimeLogged;
    stats.lifetimeUploaded = _lifetimeUploaded;
    return stats;
}

// =============================================================================
// appendReading — serialize and write one reading to flash
// =============================================================================

bool ReadingBuffer::appendReading(const Reading& r) {
    xSemaphoreTake((SemaphoreHandle_t)_mutex, portMAX_DELAY);

    File f = LittleFS.open(READINGS_FILE, "a");
    if (!f) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
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
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return false;
    }

    // Append a status byte (0 = pending) to status file.
    File sf = LittleFS.open(STATUS_FILE, "a");
    if (!sf) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return false;
    }
    uint8_t pending = 0;
    size_t sw = sf.write(&pending, 1);
    sf.close();
    if (sw != 1) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return false;
    }

    _depth++;
    _lifetimeLogged++;

    bool ok = _saveIndex();
    xSemaphoreGive((SemaphoreHandle_t)_mutex);
    return ok;
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
    xSemaphoreTake((SemaphoreHandle_t)_mutex, portMAX_DELAY);

    if (_depth == 0 || count == 0) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return 0;
    }

    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return 0;
    }

    File rf = LittleFS.open(READINGS_FILE, "r");
    if (!rf) {
        sf.close();
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return 0;
    }

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
    xSemaphoreGive((SemaphoreHandle_t)_mutex);
    return found;
}

// =============================================================================
// markUploaded — set status bytes to 1 for the given reading IDs
// =============================================================================

void ReadingBuffer::markUploaded(const uint32_t* ids, uint32_t count) {
    xSemaphoreTake((SemaphoreHandle_t)_mutex, portMAX_DELAY);

    if (count == 0 || _depth == 0) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    // Read entire status file into memory
    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    size_t statusSize = sf.size();
    if (statusSize == 0) {
        sf.close();
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    uint8_t* statusBuf = new (std::nothrow) uint8_t[statusSize];
    if (statusBuf == nullptr) {
        sf.close();
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

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
    xSemaphoreGive((SemaphoreHandle_t)_mutex);
}

// =============================================================================
// pruneUploaded — compact storage by removing all uploaded readings
// =============================================================================

void ReadingBuffer::pruneUploaded() {
    xSemaphoreTake((SemaphoreHandle_t)_mutex, portMAX_DELAY);

    if (_uploadedInBuffer == 0) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    // Read entire status file to identify which readings to keep
    File sf = LittleFS.open(STATUS_FILE, "r");
    if (!sf) {
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    size_t statusSize = sf.size();
    if (statusSize == 0) {
        sf.close();
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    uint8_t* statusBuf = new (std::nothrow) uint8_t[statusSize];
    if (statusBuf == nullptr) {
        sf.close();
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    sf.read(statusBuf, statusSize);
    sf.close();

    // Count how many uploaded readings we'll prune
    uint32_t pruneCount = 0;
    for (size_t i = 0; i < statusSize; i++) {
        if (statusBuf[i] == 1) pruneCount++;
    }

    if (pruneCount == 0) {
        delete[] statusBuf;
        xSemaphoreGive((SemaphoreHandle_t)_mutex);
        return;
    }

    // --- Compact: copy only pending readings to temp files ---
    {
        File src = LittleFS.open(READINGS_FILE, "r");
        File rdst = LittleFS.open("/readings_tmp.bin", "w");
        File sdst = LittleFS.open("/status_tmp.bin", "w");
        if (!src || !rdst || !sdst) {
            src.close(); rdst.close(); sdst.close();
            delete[] statusBuf;
            xSemaphoreGive((SemaphoreHandle_t)_mutex);
            return;
        }

        uint8_t record[READING_BINARY_SIZE];
        uint8_t pending = 0;

        for (size_t i = 0; i < statusSize; i++) {
            if (statusBuf[i] == 0) {
                // Pending reading — seek, read, and copy to temp files
                if (!src.seek(static_cast<size_t>(i) * READING_BINARY_SIZE)) break;
                if (src.read(record, READING_BINARY_SIZE) != READING_BINARY_SIZE) break;
                rdst.write(record, READING_BINARY_SIZE);
                sdst.write(&pending, 1);
            }
        }

        src.close();
        rdst.close();
        sdst.close();
    }

    delete[] statusBuf;

    // --- Swap files ---
    LittleFS.remove(READINGS_FILE);
    LittleFS.rename("/readings_tmp.bin", READINGS_FILE);
    LittleFS.remove(STATUS_FILE);
    LittleFS.rename("/status_tmp.bin", STATUS_FILE);

    // Update counters — pruned readings were all uploaded
    _depth = (pruneCount >= _depth) ? 0 : _depth - pruneCount;
    _uploadedInBuffer = (pruneCount >= _uploadedInBuffer) ? 0 : _uploadedInBuffer - pruneCount;
    _saveIndex();

    xSemaphoreGive((SemaphoreHandle_t)_mutex);
}

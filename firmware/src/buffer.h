#pragma once
#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// =============================================================================
// RadiaLog Firmware - Reading Record Definition
// =============================================================================

// Binary serialization size for on-disk storage (compact packed format).
// The in-memory struct uses full-width types for convenience; the binary
// writer (buffer.cpp) packs fields into this footprint:
//   float lat(4) + float lon(4) + float dose_rate(4) + float count_rate(4)
//   + uint32_t timestamp(4) + float accuracy(4) + float altitude(4)
//   + uint16_t speed_mph_x10(2) + uint16_t speed_kph_x10(2)
//   + uint16_t heading_x10(2)
//   = 34 bytes total
// Note: altitude_accuracy is dropped from binary to fit 34 bytes.
// Upload status is tracked in a parallel /readings_status.bin file.
// Reading ID is not stored in binary; it is (lifetimeLogged) at time of append.
static const uint8_t READING_BINARY_SIZE = 34;

// Compile-time verification
static_assert(READING_BINARY_SIZE == 34, "READING_BINARY_SIZE must be 34 bytes");

/// A single radiation reading with GPS data and upload status.
struct Reading {
    uint32_t id;                ///< Unique monotonic reading ID
    float    lat;               ///< Latitude (degrees)
    float    lon;               ///< Longitude (degrees)
    float    dose_rate;         ///< Dose rate (µSv/h)
    float    count_rate;        ///< Count rate (CPS)
    uint32_t timestamp;         ///< Unix epoch seconds
    float    accuracy;          ///< GPS horizontal accuracy (m)
    float    altitude;          ///< Altitude (m)
    float    speed_mph;         ///< Speed (mph)
    float    speed_kph;         ///< Speed (km/h)
    float    heading;           ///< Heading (degrees, 0-360)
    float    altitude_accuracy; ///< GPS vertical accuracy (m)
    bool     uploaded;          ///< true if uploaded to server
    bool     gps_valid;         ///< true if GPS had a fix at time of reading
};

// =============================================================================
// BufferStats - Statistics returned by getStats()
// =============================================================================

/// Statistics about the reading buffer.
struct BufferStats {
    uint32_t depth;              ///< Number of readings currently in buffer
    uint32_t pending;            ///< Readings in buffer awaiting upload
    uint32_t lifetimeLogged;     ///< Total readings ever appended (including pruned)
    uint32_t lifetimeUploaded;   ///< Total readings ever marked as uploaded
};

// =============================================================================
// ReadingBuffer - LittleFS-backed reading storage
// =============================================================================

class ReadingBuffer {
public:
    ReadingBuffer();

    /// Initialize storage: mount LittleFS, create data file if absent,
    /// load or initialize stats counters from index file.
    /// Returns true on success.
    bool begin();

    /// Get current buffer statistics.
    BufferStats getStats() const;

    /// Append a reading to persistent storage.
    /// Returns true on success, false if storage full or error.
    bool appendReading(const Reading& r);

    /// Fill buf with up to count unuploaded readings (oldest first).
    /// Returns the number of readings written to buf.
    uint32_t getUnuploaded(Reading* buf, uint32_t count);

    /// Mark readings with the given IDs as uploaded.
    /// ids: array of reading IDs; count: number of IDs in array.
    void markUploaded(const uint32_t* ids, uint32_t count);

    /// Remove uploaded readings when approaching storage capacity.
    /// Never removes readings with uploaded == false.
    void pruneUploaded();

private:
    uint32_t _depth;             ///< Current number of stored readings
    uint32_t _lifetimeLogged;    ///< Total readings ever appended
    uint32_t _lifetimeUploaded;  ///< Total readings ever uploaded
    uint32_t _uploadedInBuffer;  ///< Readings in buffer already uploaded

    /// Persist stats counters to /readings_idx.bin (write-through).
    bool _saveIndex();
};

#endif // BUFFER_H

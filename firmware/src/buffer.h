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
// writer (buffer.cpp, implemented later) packs fields into this footprint:
//   uint32_t id(4) + float lat(4) + float lon(4) + float dose_rate(4)
//   + float count_rate(4) + uint32_t timestamp(4) + uint8_t accuracy(1)
//   + int16_t altitude(2) + uint8_t speed_mph(1) + uint8_t speed_kph(1)
//   + uint16_t heading(2) + uint8_t altitude_accuracy(1) + uint8_t uploaded(1)
//   + uint8_t _reserved(1)
//   = 34 bytes total
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
};

// =============================================================================
// BufferStats - Statistics returned by getStats()
// =============================================================================

/// Statistics about the reading buffer.
struct BufferStats {
    uint32_t depth;              ///< Number of readings currently in buffer
    uint32_t lifetimeLogged;     ///< Total readings ever appended (including pruned)
    uint32_t lifetimeUploaded;   ///< Total readings ever marked as uploaded
};

// =============================================================================
// ReadingBuffer - LittleFS-backed reading storage
// =============================================================================

class ReadingBuffer {
public:
    ReadingBuffer();

    /// Initialize storage. Sets counters to 0.
    /// Full implementation will mount LittleFS and load index.
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
};

#endif // BUFFER_H

#pragma once
#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// RadiaLog - Reading Record Definition
// =============================================================================

// Binary serialization size for on-disk storage (compact packed format).
// The in-memory struct uses full-width types for convenience; the binary
// writer (buffer.cpp, implemented later) packs fields into this footprint:
//   float lat(4) + float lon(4) + float dose_rate(4) + float count_rate(4)
//   + uint32_t timestamp(4) + uint8_t accuracy(1) + int16_t altitude(2)
//   + uint8_t speed_mph(1) + uint8_t speed_kph(1) + uint16_t heading(2)
//   + uint8_t altitude_accuracy(1) + uint8_t uploaded(1)
//   + uint8_t _reserved(1)
//   = 30 bytes data + 4-byte id prefix = 34 bytes total
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
// ReadingBuffer - LittleFS-backed reading storage
// =============================================================================

class ReadingBuffer {
public:
    ReadingBuffer();

    /// Initialize storage (mount LittleFS, load index).
    void begin();

    /// Append a reading to persistent storage.
    /// Returns true on success, false if storage full or error.
    bool append(const Reading& r);

    /// Total number of stored readings.
    uint32_t count() const;

    /// Number of readings not yet uploaded.
    uint32_t unsentCount() const;

    /// Mark `count` unsent readings as sent (from oldest).
    void markSent(uint32_t count);

    /// Fill `buf` with up to `max` unsent readings.
    /// Returns the number of readings written.
    uint32_t getUnsent(Reading* buf, uint32_t max);

private:
    uint32_t _count;
    uint32_t _unsentCount;
};

#endif // BUFFER_H

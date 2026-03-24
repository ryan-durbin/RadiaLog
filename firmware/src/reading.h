#pragma once
#ifndef READING_H
#define READING_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// RadiaLog Firmware - Canonical Reading Struct
// Used by main.cpp integration to merge GPS + radiation data
// =============================================================================

/// A single merged radiation + GPS reading record.
struct Reading {
    uint32_t timestamp;           ///< Unix epoch seconds
    double   lat;                 ///< Latitude (degrees, double precision)
    double   lon;                 ///< Longitude (degrees, double precision)
    float    dose_rate;           ///< Dose rate (µSv/h)
    float    count_rate;          ///< Count rate (CPS)
    float    altitude;            ///< Altitude (m)
    float    speed_mph;           ///< Speed (mph)
    float    speed_kph;           ///< Speed (km/h)
    float    heading;             ///< Heading (degrees, 0-360)
    float    accuracy;            ///< GPS horizontal accuracy (m)
    float    altitude_accuracy;   ///< GPS vertical accuracy (m)
    bool     gps_valid;           ///< true if GPS had a fix at time of reading
};

#endif // READING_H

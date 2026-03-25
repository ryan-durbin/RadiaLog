#pragma once
#ifndef RADIACODE_MGR_H
#define RADIACODE_MGR_H

#include <Arduino.h>
#include <vector>
#include "radiacode.h"
#include "radiacode_ble.h"

// =============================================================================
// RadiaLog Firmware - RadiaCode Device Manager
// Manages a single USB device + multiple BLE devices. Polls all connected
// devices for readings and provides a unified interface to the main loop.
// =============================================================================

namespace radiacode {

/// A reading tagged with the source device identifier.
struct TaggedReading {
    String   deviceId;     // "USB" or BLE MAC address
    float    dose_rate;
    float    count_rate;
};

class RadiaCodeMgr {
public:
    static constexpr int MAX_BLE_DEVICES = 4;

    RadiaCodeMgr();

    /// Initialize the manager. Owns USB device lifetime; BLE MACs from config.
    void begin(RadiaCode& usbDevice, const std::vector<String>& bleMacs);

    /// Poll all connected devices. Call once per loop iteration.
    /// Attempts reconnect on disconnected BLE devices periodically.
    /// Returns tagged readings from all devices that had data.
    std::vector<TaggedReading> poll();

    /// Total number of currently connected devices (USB + BLE).
    int connectedCount() const;

    /// True if the USB device is connected.
    bool isUsbConnected() const;

    /// Number of BLE devices currently connected.
    int bleConnectedCount() const;

    /// Number of configured BLE devices (connected or not).
    int bleConfiguredCount() const;

private:
    RadiaCode* _usb;

    struct BLEEntry {
        String        macAddress;
        RadiaCodeBLE  device;
        bool          initialized;
        unsigned long lastReconnectAttemptMs;
    };

    std::vector<BLEEntry> _bleDevices;

    static constexpr unsigned long BLE_RECONNECT_INTERVAL_MS = 15000;
};

} // namespace radiacode

#endif // RADIACODE_MGR_H

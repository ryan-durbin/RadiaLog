#include "radiacode_mgr.h"
#include "portal/debug_ws.h"

// =============================================================================
// RadiaLog Firmware - RadiaCode Device Manager Implementation
// Manages USB + BLE RadiaCode connections. Polls all devices each loop
// iteration and returns combined readings.
// =============================================================================

namespace radiacode {

RadiaCodeMgr::RadiaCodeMgr()
    : _usb(nullptr)
{
}

void RadiaCodeMgr::begin(RadiaCode& usbDevice, const std::vector<String>& bleMacs) {
    _usb = &usbDevice;

    // Initialize BLE stack if we have BLE devices configured
    if (!bleMacs.empty()) {
        RadiaCodeBLE::initBLE();
        debugWS.log(MOD_USB, LVL_INFO,
            "[RadiaCodeMgr] BLE initialized. " + String(bleMacs.size())
            + " device(s) configured.");
    }

    // Set up BLE device entries
    _bleDevices.clear();
    for (const auto& mac : bleMacs) {
        if (mac.length() == 0) continue;
        if (_bleDevices.size() >= MAX_BLE_DEVICES) break;

        BLEEntry entry;
        entry.macAddress = mac;
        entry.initialized = false;
        entry.lastReconnectAttemptMs = 0;
        _bleDevices.push_back(std::move(entry));

        debugWS.log(MOD_USB, LVL_INFO,
            "[RadiaCodeMgr] BLE device added: " + mac);
    }

    // Attempt initial BLE connections
    for (auto& entry : _bleDevices) {
        Error err = entry.device.connect(entry.macAddress);
        if (err == Error::OK) {
            bool ok = entry.device.init();
            entry.initialized = ok;
            if (ok) {
                debugWS.log(MOD_USB, LVL_INFO,
                    "[RadiaCodeMgr] BLE device " + entry.macAddress + " ready.");
            }
        }
        entry.lastReconnectAttemptMs = millis();
    }
}

std::vector<TaggedReading> RadiaCodeMgr::poll() {
    std::vector<TaggedReading> results;

    // --- Poll USB device ---
    if (_usb != nullptr) {
        if (_usb->isConnected()) {
            auto readings = _usb->readDataBuf();
            if (!readings.empty()) {
                auto& latest = readings.back();
                TaggedReading tr;
                tr.deviceId   = "USB";
                tr.dose_rate  = latest.dose_rate;
                tr.count_rate = latest.count_rate;
                results.push_back(tr);
            }
        } else {
            // Attempt USB reconnect (existing behavior from main.cpp)
            Error err = _usb->connect();
            if (err == Error::OK) {
                _usb->init();
                debugWS.log(MOD_USB, LVL_INFO,
                    "[RadiaCodeMgr] USB device reconnected.");
            }
        }
    }

    // --- Poll BLE devices ---
    for (auto& entry : _bleDevices) {
        if (entry.device.isConnected() && entry.initialized) {
            auto readings = entry.device.readDataBuf();
            if (!readings.empty()) {
                auto& latest = readings.back();
                TaggedReading tr;
                tr.deviceId   = entry.macAddress;
                tr.dose_rate  = latest.dose_rate;
                tr.count_rate = latest.count_rate;
                results.push_back(tr);
            }
        } else {
            // Attempt BLE reconnect periodically
            unsigned long now = millis();
            if ((now - entry.lastReconnectAttemptMs) >= BLE_RECONNECT_INTERVAL_MS) {
                entry.lastReconnectAttemptMs = now;

                // Disconnect cleanly first if partially connected
                if (entry.device.isConnected()) {
                    entry.device.disconnect();
                }

                Error err = entry.device.connect(entry.macAddress);
                if (err == Error::OK) {
                    bool ok = entry.device.init();
                    entry.initialized = ok;
                    if (ok) {
                        debugWS.log(MOD_USB, LVL_INFO,
                            "[RadiaCodeMgr] BLE " + entry.macAddress + " reconnected.");
                    }
                }
            }
        }
    }

    return results;
}

int RadiaCodeMgr::connectedCount() const {
    int count = 0;
    if (_usb && _usb->isConnected()) count++;
    for (const auto& entry : _bleDevices) {
        if (entry.device.isConnected() && entry.initialized) count++;
    }
    return count;
}

bool RadiaCodeMgr::isUsbConnected() const {
    return _usb && _usb->isConnected();
}

int RadiaCodeMgr::bleConnectedCount() const {
    int count = 0;
    for (const auto& entry : _bleDevices) {
        if (entry.device.isConnected() && entry.initialized) count++;
    }
    return count;
}

int RadiaCodeMgr::bleConfiguredCount() const {
    return static_cast<int>(_bleDevices.size());
}

} // namespace radiacode

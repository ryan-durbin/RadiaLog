#include "radiacode_mgr.h"
#include "portal/debug_ws.h"

// =============================================================================
// RadiaLog Firmware - RadiaCode Device Manager Implementation
// Manages USB + BLE RadiaCode connections. Polls all devices each loop
// iteration and returns combined readings. BLE reconnection runs in a
// background FreeRTOS task to avoid blocking the main loop.
// =============================================================================

namespace radiacode {

/// Context passed to the one-shot BLE reconnect FreeRTOS task.
struct BleReconnectCtx {
    RadiaCodeBLE* device;
    String        macAddress;
    volatile bool* initialized;
    volatile bool* reconnecting;
};

RadiaCodeMgr::RadiaCodeMgr()
    : _usb(nullptr)
{
}

void RadiaCodeMgr::begin(RadiaCode& usbDevice, const std::vector<String>& bleMacs) {
    _usb = &usbDevice;

    // Initialize BLE stack if we have BLE devices configured
    if (!bleMacs.empty()) {
        RadiaCodeBLE::initBLE();
        debugWS.log(MOD_BLE, LVL_INFO,
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
        entry.reconnecting = false;
        _bleDevices.push_back(std::move(entry));

        debugWS.log(MOD_BLE, LVL_INFO,
            "[RadiaCodeMgr] BLE device added: " + mac);
    }

    // Kick off initial BLE connections in background tasks
    for (auto& entry : _bleDevices) {
        entry.lastReconnectAttemptMs = millis();
        _startBleReconnectTask(entry);
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
                    "[RadiaCodeMgr] USB reconnected.");
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
        } else if (!entry.device.isConnected() && entry.initialized) {
            // Was connected but lost connection — reset state
            debugWS.log(MOD_BLE, LVL_WARN,
                "[RadiaCodeMgr] BLE " + entry.macAddress + " connection lost");
            entry.initialized = false;
        }
        if (!entry.device.isConnected() && !entry.initialized && !entry.reconnecting) {
            // Launch background reconnect if interval has elapsed
            unsigned long now = millis();
            if ((now - entry.lastReconnectAttemptMs) >= BLE_RECONNECT_INTERVAL_MS) {
                entry.lastReconnectAttemptMs = now;
                _startBleReconnectTask(entry);
            }
        }
    }

    return results;
}

// =============================================================================
// Background BLE reconnect task — runs on core 0, deletes itself when done
// =============================================================================

void RadiaCodeMgr::_startBleReconnectTask(BLEEntry& entry) {
    entry.reconnecting = true;

    // Heap-allocate context so the task can outlive this stack frame
    auto* ctx = new BleReconnectCtx();
    ctx->device       = &entry.device;
    ctx->macAddress    = entry.macAddress;
    ctx->initialized   = &entry.initialized;
    ctx->reconnecting  = &entry.reconnecting;

    xTaskCreatePinnedToCore(
        [](void* param) {
            auto* c = static_cast<BleReconnectCtx*>(param);

            // Disconnect cleanly first if partially connected
            if (c->device->isConnected()) {
                c->device->disconnect();
            }

            Error err = c->device->connect(c->macAddress);
            if (err == Error::OK) {
                bool ok = c->device->init();
                *c->initialized = ok;
                if (ok) {
                    debugWS.log(MOD_BLE, LVL_INFO,
                        "[RadiaCodeMgr] BLE " + c->macAddress + " connected.");
                } else {
                    debugWS.log(MOD_BLE, LVL_WARN,
                        "[RadiaCodeMgr] BLE " + c->macAddress + " init failed.");
                }
            } else {
                debugWS.log(MOD_BLE, LVL_WARN,
                    "[RadiaCodeMgr] BLE " + c->macAddress + " connect failed.");
            }

            *c->reconnecting = false;
            delete c;
            vTaskDelete(nullptr);
        },
        "ble_reconn", 8192, ctx, 1, nullptr, 0  // core 0, away from main loop
    );
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

void RadiaCodeMgr::updateBleDevices(const std::vector<String>& bleMacs) {
    // Disconnect all existing BLE devices
    for (auto& entry : _bleDevices) {
        if (entry.device.isConnected()) {
            entry.device.disconnect();
        }
    }
    _bleDevices.clear();

    if (bleMacs.empty()) return;

    // Initialize BLE stack if needed
    RadiaCodeBLE::initBLE();

    // Set up new device entries
    for (const auto& mac : bleMacs) {
        if (mac.length() == 0) continue;
        if (_bleDevices.size() >= MAX_BLE_DEVICES) break;

        BLEEntry entry;
        entry.macAddress = mac;
        entry.initialized = false;
        entry.lastReconnectAttemptMs = millis();
        entry.reconnecting = false;
        _bleDevices.push_back(std::move(entry));

        debugWS.log(MOD_BLE, LVL_INFO,
            "[RadiaCodeMgr] BLE device added: " + mac);
    }

    // Kick off initial connections in background tasks
    for (auto& entry : _bleDevices) {
        _startBleReconnectTask(entry);
    }
}

} // namespace radiacode

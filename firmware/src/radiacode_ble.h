#pragma once
#ifndef RADIACODE_BLE_H
#define RADIACODE_BLE_H

#include <Arduino.h>
#include <stdint.h>
#include <vector>
#include "radiacode.h"  // Reuse RadiaCodeReading, COMMAND, VS, Error, parseDataBuf

// =============================================================================
// RadiaLog Firmware - RadiaCode BLE Transport
// Connects to RadiaCode devices via BLE using the same command/response protocol
// as USB. Uses NimBLE-Arduino for the BLE client stack.
//
// BLE Service:            e63215e5-7003-49d8-96b0-b024798fb901
// Write Characteristic:   e63215e6-7003-49d8-96b0-b024798fb901 (write-without-response)
// Notify Characteristic:  e63215e7-7003-49d8-96b0-b024798fb901 (notify — responses)
// =============================================================================

#ifdef ARDUINO
#include <NimBLEDevice.h>
#endif

namespace radiacode {

class RadiaCodeBLE {
public:
    // RadiaCode BLE UUIDs
    static constexpr const char* SERVICE_UUID    = "e63215e5-7003-49d8-96b0-b024798fb901";
    static constexpr const char* WRITE_CHAR_UUID = "e63215e6-7003-49d8-96b0-b024798fb901";
    static constexpr const char* NOTIFY_CHAR_UUID = "e63215e7-7003-49d8-96b0-b024798fb901";

    // BLE write chunk size (default ATT_MTU 23 - 3 ATT overhead - 2 handle = 18)
    static constexpr size_t BLE_WRITE_CHUNK = 18;

    static constexpr uint32_t BLE_TIMEOUT_MS = 5000;

    RadiaCodeBLE();
    ~RadiaCodeBLE();

    // Move-only: semaphore ownership must transfer cleanly
    RadiaCodeBLE(RadiaCodeBLE&& other) noexcept;
    RadiaCodeBLE& operator=(RadiaCodeBLE&& other) noexcept;
    RadiaCodeBLE(const RadiaCodeBLE&) = delete;
    RadiaCodeBLE& operator=(const RadiaCodeBLE&) = delete;

    /// Initialize the NimBLE stack. Call once before creating any instances.
    static void initBLE();

    /// Connect to a RadiaCode by BLE MAC address.
    Error connect(const String& macAddress);

    /// Disconnect from the device.
    void disconnect();

    /// Returns true if connected.
    bool isConnected() const { return _connected; }

    /// Run the RadiaCode init sequence (same as USB: SET_EXCHANGE, SET_TIME, etc.)
    bool init();

    /// Read and decode the data buffer. Returns decoded readings.
    std::vector<RadiaCodeReading> readDataBuf();

    /// Get the MAC address this instance is targeting.
    String getMacAddress() const { return _macAddress; }

    /// Called by the notification callback to deliver response data.
    void onNotify(const uint8_t* data, size_t len);

private:
    String _macAddress;
    bool   _connected;
    uint8_t _seq;
    uint32_t _base_time;

#ifdef ARDUINO
    NimBLEClient*               _client;
    NimBLERemoteCharacteristic* _writeChar;
    NimBLERemoteCharacteristic* _notifyChar;
    SemaphoreHandle_t           _responseSemaphore;
#endif

    // Response accumulation buffer
    std::vector<uint8_t> _responseBuffer;
    uint32_t             _expectedResponseLen;
    volatile bool        _responseReady;

    // Transport
    Error execute(const std::vector<uint8_t>& request,
                  std::vector<uint8_t>& response);

    // Protocol helpers (same logic as USB RadiaCode)
    Error sendCommand(COMMAND cmd, const std::vector<uint8_t>& args,
                      std::vector<uint8_t>& response);
    std::vector<uint8_t> buildRequest(uint16_t command_id,
                                       const std::vector<uint8_t>& args = {});
    std::vector<uint8_t> read_request(VS vs_id);
    std::vector<uint8_t> write_request(uint16_t vsfr_id, uint32_t value);

    static void packLE32(uint8_t* buf, uint32_t val);
    static uint32_t unpackLE32(const uint8_t* buf);
    uint32_t getCurrentTimeSec();
};

} // namespace radiacode

#endif // RADIACODE_BLE_H

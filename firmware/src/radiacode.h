#pragma once

// =============================================================================
// RadiaCode USB Transport Layer
// Board: Seeed XIAO ESP32S3
// Protocol: USB Bulk, VID=0x0483 / PID=0xF123
// =============================================================================
// Reference: docs/reference/usb_transport.py
// Reference: docs/reference/radiacode_protocol.md
//
// Transport framing:
//   Request:  [4-byte LE uint32 length][command_id(2) + 0x00 + seq(1) + args]
//   Response: [4-byte LE uint32 length][response_data]
// Seq: 0x80 + (seq % 32), increments after each execute()
// =============================================================================

#include <Arduino.h>
#include <stdint.h>
#include <vector>

// Forward declare ESP-IDF USB Host types to avoid pulling in the full headers
// at include time (they define C structs that conflict with Arduino framework
// in some build paths). The .cpp includes usb/usb_host.h directly.
struct usb_host_client_event_msg_t;
typedef void* usb_host_client_handle_t;
typedef void* usb_device_handle_t;
typedef void* usb_transfer_t;

namespace radiacode {

// ---- Error codes -------------------------------------------------------

enum class Error {
    OK = 0,
    DEVICE_NOT_FOUND,
    CONNECT_FAILED,
    WRITE_FAILED,
    READ_TIMEOUT,
    READ_FAILED,
    MULTIPLE_READ_FAILURES,
    NOT_CONNECTED,
};

// ---- RadiaCode USB transport class -------------------------------------

class RadiaCode {
public:
    // Timeout for USB bulk reads (milliseconds)
    static constexpr uint32_t USB_TIMEOUT_MS = 3000;

    // Maximum retries for zero-length reads before giving up
    static constexpr int MAX_READ_RETRIES = 3;

    // Maximum packet size for USB bulk transfers
    static constexpr int USB_PACKET_SIZE = 256;

    // Drain reads: keep reading until timeout, up to this many packets
    static constexpr int DRAIN_MAX_PACKETS = 64;

    RadiaCode();
    ~RadiaCode();

    // Connect to the RadiaCode device (VID=0x0483, PID=0xF123).
    // Drains stale data from the device on success.
    // Returns Error::OK on success, or an error code.
    Error connect();

    // Disconnect from the device and release USB Host resources.
    void disconnect();

    // Returns true if a device is currently connected.
    bool isConnected() const { return _connected; }

    // Execute a command: write a raw request payload, read and parse response.
    // Builds the length-prefixed framing around `payload`.
    // On success, fills `response` with the raw response bytes (no length prefix).
    // Returns Error::OK on success, or an error code.
    Error execute(const std::vector<uint8_t>& payload,
                  std::vector<uint8_t>& response);

    // Build a request payload: [cmd_id(2 LE)][0x00][seq_byte][args...]
    // Prepends the 4-byte LE length prefix.
    // Increments the internal sequence counter.
    std::vector<uint8_t> buildRequest(uint16_t command_id,
                                      const std::vector<uint8_t>& args = {});

    // Current sequence number (0-31)
    uint8_t getSeq() const { return _seq; }

private:
    bool _connected;
    uint8_t _seq;               // 0-31, increments on each buildRequest()

    usb_host_client_handle_t _client_hdl;
    usb_device_handle_t _dev_hdl;
    usb_transfer_t* _out_xfer;  // re-usable OUT transfer buffer
    usb_transfer_t* _in_xfer;   // re-usable IN transfer buffer

    // Write raw bytes to USB write endpoint (0x01).
    Error usbWrite(const uint8_t* data, size_t len);

    // Read up to max_len bytes from USB read endpoint (0x81).
    // Returns number of bytes actually read, or -1 on error / timeout.
    int usbRead(uint8_t* buf, size_t max_len, uint32_t timeout_ms = USB_TIMEOUT_MS);

    // Drain stale data: read until timeout, discard all bytes.
    void drainStaleData();

    // Install USB Host library and register client (called once in connect()).
    Error initUsbHost();

    // Scan connected devices for VID/PID match and open the device.
    // Returns Error::OK and sets _dev_hdl on success.
    Error findAndOpenDevice();

    // Claim the bulk interface and configure endpoints.
    Error claimInterface();

    // Release USB Host resources.
    void releaseUsbHost();

    // Helper: pack a uint32_t into 4 little-endian bytes.
    static void packLE32(uint8_t* buf, uint32_t val);

    // Helper: unpack a uint32_t from 4 little-endian bytes.
    static uint32_t unpackLE32(const uint8_t* buf);
};

} // namespace radiacode

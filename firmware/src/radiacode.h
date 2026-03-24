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

// ---- Protocol command IDs ----------------------------------------------

enum class COMMAND : uint16_t {
    SET_EXCHANGE    = 0x0007,
    SET_TIME        = 0x0A04,
    GET_VERSION     = 0x000A,
    GET_SERIAL      = 0x000B,
    RD_VIRT_SFR     = 0x0824,
    WR_VIRT_SFR     = 0x0825,
    RD_VIRT_STRING  = 0x0826,
    WR_VIRT_STRING  = 0x0827,
};

// ---- Virtual store (VS) identifiers ------------------------------------

enum class VS : uint16_t {
    CONFIGURATION  = 0x0002,
    SERIAL_NUMBER  = 0x0008,
    DATA_BUF       = 0x0100,
    SPECTRUM       = 0x0200,
};

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

    // Convenience: build + execute a command in one call.
    // Returns Error::OK and fills `response` on success.
    Error sendCommand(COMMAND command_id,
                      const std::vector<uint8_t>& args,
                      std::vector<uint8_t>& response);

    // Build a RD_VIRT_STRING request payload for reading virtual store `vs_id`.
    // Does NOT call execute(); returns the raw framed request bytes.
    std::vector<uint8_t> read_request(VS vs_id);

    // Build a WR_VIRT_SFR request payload for writing VSFR register `vsfr_id`
    // with `value` (4-byte LE uint32). Does NOT call execute().
    std::vector<uint8_t> write_request(uint16_t vsfr_id, uint32_t value);

    // Initialize the RadiaCode device after USB connection.
    // Performs the full init sequence:
    //   1. drain stale data
    //   2. SET_EXCHANGE(0x0007) with payload 0x01ff12ff
    //   3. SET_TIME(0x0A04) with current datetime
    //   4. Write DEVICE_TIME to VSFR 0x0504 with value 0
    //   5. Compute and store base_time = now() + 128 seconds
    //   6. Read and log firmware version (GET_VERSION)
    // Returns true on success, false on any failure.
    bool init();

    // Current sequence number (0-31)
    uint8_t getSeq() const { return _seq; }

    // base_time in Unix seconds (set during init(), used for data buffer decoding)
    uint32_t getBaseTime() const { return _base_time; }

private:
    bool _connected;
    uint8_t _seq;               // 0-31, increments on each buildRequest()
    uint32_t _base_time;        // Unix seconds; set in init() = now() + 128s

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

    // Get current time as Unix seconds (wraps time()/millis() on device;
    // returns fixed value in unit tests).
    uint32_t getCurrentTimeSec();
};

} // namespace radiacode

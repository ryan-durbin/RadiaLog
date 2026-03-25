// =============================================================================
// RadiaCode USB Transport Layer — Implementation
// Board: Seeed XIAO ESP32S3 (ESP-IDF USB Host API)
// =============================================================================
// Reference: docs/reference/usb_transport.py
// Reference: docs/reference/radiacode_protocol.md
//
// Transport framing:
//   Request:  [4-byte LE uint32 length][command_id(2) + 0x00 + seq(1) + args]
//   Response: [4-byte LE uint32 length][response_data]
// Seq: 0x80 + (seq % 32), increments after each buildRequest()
//
// NOTE: PlatformIO not yet installed on this host. This file is syntactically
// valid C++ intended for compilation with the Arduino/ESP-IDF toolchain.
// The USB Host API calls are wrapped in #ifdef guards so that the logic can
// be exercised in unit tests without the full ESP-IDF headers.
// =============================================================================

#include "radiacode.h"

// Only include ESP-IDF USB Host headers when building for the target.
#ifdef ARDUINO
#  include <usb/usb_host.h>
#endif

#include <cstring>
#include <cstdio>

// Redefine nullptr-safe sentinel for transfer handles outside Arduino.
#ifndef ARDUINO
#  define portMAX_DELAY 0xFFFFFFFFUL
#endif

namespace radiacode {

// ============================================================================
// Static helpers
// ============================================================================

void RadiaCode::packLE32(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

uint32_t RadiaCode::unpackLE32(const uint8_t* buf) {
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

RadiaCode::RadiaCode()
    : _connected(false)
    , _seq(0)
    , _base_time(0)
    , _client_hdl(nullptr)
    , _dev_hdl(nullptr)
    , _out_xfer(nullptr)
    , _in_xfer(nullptr)
    , _xfer_semaphore(nullptr)
{}

RadiaCode::~RadiaCode() {
    disconnect();
}

// ============================================================================
// Public API
// ============================================================================

Error RadiaCode::connect() {
    if (_connected) {
        return Error::OK;
    }

    Error err = initUsbHost();
    if (err != Error::OK) {
        return err;
    }

    err = findAndOpenDevice();
    if (err != Error::OK) {
        releaseUsbHost();
        return err;
    }

    err = claimInterface();
    if (err != Error::OK) {
        releaseUsbHost();
        return err;
    }

    _connected = true;
    _seq = 0;

    // Drain any stale data buffered in the device before starting
    drainStaleData();

    return Error::OK;
}

void RadiaCode::disconnect() {
    if (!_connected && _client_hdl == nullptr) {
        return;
    }
    _connected = false;
    releaseUsbHost();
}

bool RadiaCode::init() {
    if (!_connected) {
        return false;
    }

    // ---- Step 1: drain stale data ----------------------------------------
    drainStaleData();

    // ---- Step 2: SET_EXCHANGE with payload 0x01 0xff 0x12 0xff -------------
    {
        std::vector<uint8_t> args = { 0x01, 0xff, 0x12, 0xff };
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::SET_EXCHANGE, args, resp);
        if (err != Error::OK) {
            return false;
        }
    }

    // ---- Step 3: SET_TIME with current time (Unix timestamp LE uint32) -----
    {
        uint32_t now_sec = getCurrentTimeSec();
        std::vector<uint8_t> args(4);
        packLE32(args.data(), now_sec);
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::SET_TIME, args, resp);
        if (err != Error::OK) {
            return false;
        }
    }

    // ---- Step 4: Write DEVICE_TIME to VSFR 0x0504 with value 0 -----------
    {
        std::vector<uint8_t> req = write_request(0x0504, 0);
        std::vector<uint8_t> resp;
        Error err = execute(req, resp);
        if (err != Error::OK) {
            return false;
        }
    }

    // ---- Step 5: Compute base_time = now() + 128 seconds ------------------
    _base_time = getCurrentTimeSec() + 128;

    // ---- Step 6: Read firmware version (GET_VERSION) and log it -----------
    {
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::GET_VERSION, {}, resp);
        if (err == Error::OK && !resp.empty()) {
            // Version string is null-terminated text in response bytes
#ifdef ARDUINO
            char ver_str[64] = {0};
            size_t copy_len = resp.size() < sizeof(ver_str) - 1 ? resp.size() : sizeof(ver_str) - 1;
            memcpy(ver_str, resp.data(), copy_len);
            Serial.print("[RadiaCode] Firmware version: ");
            Serial.println(ver_str);
#endif
        }
        // Non-fatal: proceed even if GET_VERSION fails
    }

    return true;
}

// Internal helper: returns current time as Unix seconds.
// On Arduino: uses ESP-IDF time(). In unit tests: returns a fixed value.
uint32_t RadiaCode::getCurrentTimeSec() {
#ifdef ARDUINO
    return static_cast<uint32_t>(time(nullptr));
#else
    // In unit-test context, return a known fixed value
    return 1700000000UL;  // 2023-11-14 22:13:20 UTC (arbitrary, deterministic)
#endif
}

std::vector<uint8_t> RadiaCode::buildRequest(uint16_t command_id,
                                              const std::vector<uint8_t>& args) {
    // Payload: [cmd_id_lo][cmd_id_hi][0x00][seq_byte][...args]
    uint8_t seq_byte = static_cast<uint8_t>(0x80 + (_seq % 32));
    _seq = (_seq + 1) % 32;

    std::vector<uint8_t> payload;
    payload.reserve(4 + args.size());
    payload.push_back(static_cast<uint8_t>(command_id & 0xFF));        // cmd_id lo
    payload.push_back(static_cast<uint8_t>((command_id >> 8) & 0xFF)); // cmd_id hi
    payload.push_back(0x00);
    payload.push_back(seq_byte);
    payload.insert(payload.end(), args.begin(), args.end());

    // Prepend 4-byte LE length prefix
    std::vector<uint8_t> request(4 + payload.size());
    packLE32(request.data(), static_cast<uint32_t>(payload.size()));
    std::memcpy(request.data() + 4, payload.data(), payload.size());

    return request;
}

Error RadiaCode::execute(const std::vector<uint8_t>& request,
                         std::vector<uint8_t>& response) {
    if (!_connected) {
        return Error::NOT_CONNECTED;
    }

    // --- Write request ---
    Error werr = usbWrite(request.data(), request.size());
    if (werr != Error::OK) {
        return werr;
    }

    // --- Read response with retry for zero-length reads ---
    uint8_t header_buf[USB_PACKET_SIZE];
    int retries = 0;
    int header_bytes = 0;

    while (retries < MAX_READ_RETRIES) {
        header_bytes = usbRead(header_buf, sizeof(header_buf));
        if (header_bytes > 0) {
            break;
        }
        retries++;
    }

    if (retries >= MAX_READ_RETRIES) {
        return Error::MULTIPLE_READ_FAILURES;
    }

    if (header_bytes < 0) {
        return Error::READ_TIMEOUT;
    }

    if (header_bytes < 4) {
        return Error::READ_FAILED;
    }

    // Parse 4-byte LE length prefix
    uint32_t response_length = unpackLE32(header_buf);

    // Copy any bytes already read beyond the header
    std::vector<uint8_t> data(header_buf + 4, header_buf + header_bytes);

    // Keep reading until we have all response_length bytes
    while (data.size() < response_length) {
        uint8_t chunk_buf[USB_PACKET_SIZE];
        size_t remaining = response_length - data.size();
        size_t to_read = (remaining < USB_PACKET_SIZE) ? remaining : USB_PACKET_SIZE;

        int n = usbRead(chunk_buf, to_read);
        if (n <= 0) {
            return Error::READ_TIMEOUT;
        }
        data.insert(data.end(), chunk_buf, chunk_buf + n);
    }

    response = std::move(data);
    return Error::OK;
}

Error RadiaCode::sendCommand(COMMAND command_id,
                              const std::vector<uint8_t>& args,
                              std::vector<uint8_t>& response) {
    std::vector<uint8_t> request = buildRequest(
        static_cast<uint16_t>(command_id), args);
    return execute(request, response);
}

std::vector<RadiaCodeReading> RadiaCode::readDataBuf() {
    // Build the read request for VS.DATA_BUF (0x0100)
    std::vector<uint8_t> request = read_request(VS::DATA_BUF);
    std::vector<uint8_t> response;

    Error err = execute(request, response);
    if (err != Error::OK || response.empty()) {
        return {};  // Empty or error
    }

    // The response from execute() contains the raw command response.
    // Skip the 4-byte command header (cmd_id(2) + 0x00 + seq) to get data payload.
    if (response.size() <= 4) {
        return {};  // No data payload beyond header
    }

    const uint8_t* data = response.data() + 4;
    size_t data_len = response.size() - 4;

    uint64_t base_time_ms = static_cast<uint64_t>(_base_time) * 1000ULL;

    return parseDataBuf(data, data_len, base_time_ms);
}

std::vector<RadiaCodeReading> RadiaCode::parseDataBuf(const uint8_t* data,
                                                       size_t len,
                                                       uint64_t base_time_ms) {
    std::vector<RadiaCodeReading> readings;

    if (data == nullptr || len == 0) {
        return readings;
    }

    // Record header: 7 bytes
    //   seq(1) + eid(1) + gid(1) + ts_offset(4 LE int32)
    // RealTimeData payload (gid=0): 13 bytes
    //   count_rate(float4) + dose_rate(float4) + count_rate_err(uint16) +
    //   dose_rate_err(uint16) + flags(uint16) + rt_flags(uint8)
    //   = struct <ffHHHB = 4+4+2+2+2+1 = 15 bytes

    static constexpr size_t HEADER_SIZE = 7;
    static constexpr size_t REALTIME_DATA_SIZE = 15;  // ffHHHB = 4+4+2+2+2+1

    size_t offset = 0;

    while (offset + HEADER_SIZE <= len) {
        // Parse 7-byte header
        uint8_t rec_seq = data[offset];
        uint8_t eid = data[offset + 1];
        uint8_t gid = data[offset + 2];
        int32_t ts_offset;
        std::memcpy(&ts_offset, data + offset + 3, 4);
        // ts_offset is LE int32 — on little-endian ESP32 this is correct as-is
        // For portability, decode explicitly:
        ts_offset = static_cast<int32_t>(
            static_cast<uint32_t>(data[offset + 3]) |
            (static_cast<uint32_t>(data[offset + 4]) << 8) |
            (static_cast<uint32_t>(data[offset + 5]) << 16) |
            (static_cast<uint32_t>(data[offset + 6]) << 24)
        );

        offset += HEADER_SIZE;

        if (gid == 0) {
            // RealTimeData: decode struct <ffHHHB
            if (offset + REALTIME_DATA_SIZE > len) {
                break;  // Not enough data for this record
            }

            RadiaCodeReading reading;

            // count_rate: float (4 bytes LE)
            std::memcpy(&reading.count_rate, data + offset, 4);
            // dose_rate: float (4 bytes LE) — raw value scaled by 10000 to get µSv/h
            std::memcpy(&reading.dose_rate, data + offset + 4, 4);
            reading.dose_rate *= 10000.0f;
            // count_rate_err: uint16 LE
            reading.count_rate_err = static_cast<uint16_t>(data[offset + 8]) |
                                     (static_cast<uint16_t>(data[offset + 9]) << 8);
            // dose_rate_err: uint16 LE
            reading.dose_rate_err = static_cast<uint16_t>(data[offset + 10]) |
                                    (static_cast<uint16_t>(data[offset + 11]) << 8);
            // flags: uint16 LE (skip — not stored in RadiaCodeReading)
            // rt_flags: uint8 (skip — not stored in RadiaCodeReading)

            // Timestamp: base_time + ts_offset * 10 ms
            uint64_t ts_ms = base_time_ms + static_cast<int64_t>(ts_offset) * 10LL;
            reading.timestamp = static_cast<uint32_t>(ts_ms / 1000ULL);

            readings.push_back(reading);
            offset += REALTIME_DATA_SIZE;
        } else {
            // Unknown record type — we need to skip it but we don't know its size.
            // The protocol doesn't have a generic length field per record beyond the header.
            // For gid != 0 records, we skip based on known sizes.
            // gid=1: EnergySpectrum, gid=2: DoseHistory, etc.
            // For now, break out since we can't determine the payload size.
            // In practice, DATA_BUF typically only contains RealTimeData records.
            break;
        }
    }

    return readings;
}

std::vector<uint8_t> RadiaCode::read_request(VS vs_id) {
    // RD_VIRT_STRING args: 2-byte LE virtual store ID
    uint16_t id = static_cast<uint16_t>(vs_id);
    std::vector<uint8_t> args = {
        static_cast<uint8_t>(id & 0xFF),
        static_cast<uint8_t>((id >> 8) & 0xFF),
    };
    return buildRequest(static_cast<uint16_t>(COMMAND::RD_VIRT_STRING), args);
}

std::vector<uint8_t> RadiaCode::write_request(uint16_t vsfr_id, uint32_t value) {
    // WR_VIRT_SFR args: 2-byte LE VSFR id + 4-byte LE uint32 value
    std::vector<uint8_t> args(6);
    args[0] = static_cast<uint8_t>(vsfr_id & 0xFF);
    args[1] = static_cast<uint8_t>((vsfr_id >> 8) & 0xFF);
    packLE32(&args[2], value);
    return buildRequest(static_cast<uint16_t>(COMMAND::WR_VIRT_SFR), args);
}

// ============================================================================
// Private: USB I/O
// ============================================================================

#ifdef ARDUINO
// USB transfer completion callback — signals the binary semaphore from ISR context.
static void usbTransferCallback(usb_transfer_t* xfer) {
    xSemaphoreGiveFromISR((SemaphoreHandle_t)xfer->context, nullptr);
}
#endif

Error RadiaCode::usbWrite(const uint8_t* data, size_t len) {
#ifdef ARDUINO
    // ESP-IDF USB Host bulk transfer (synchronous via semaphore or direct API)
    // The actual xfer mechanism depends on the USB Host driver version.
    // For simplicity, use the synchronous usb_host_transfer_submit path.
    if (_out_xfer == nullptr || _dev_hdl == nullptr) {
        return Error::WRITE_FAILED;
    }

    // Fill transfer buffer
    usb_transfer_t* xfer = reinterpret_cast<usb_transfer_t*>(_out_xfer);
    if (len > xfer->data_buffer_size) {
        return Error::WRITE_FAILED;
    }
    std::memcpy(xfer->data_buffer, data, len);
    xfer->num_bytes = len;

    esp_err_t ret = usb_host_transfer_submit(xfer);
    if (ret != ESP_OK) {
        return Error::WRITE_FAILED;
    }

    // Wait for transfer completion (semaphore is given in usbTransferCallback)
    xSemaphoreTake((SemaphoreHandle_t)_xfer_semaphore, pdMS_TO_TICKS(USB_TIMEOUT_MS));

    return Error::OK;
#else
    // Unit-test stub: always succeed
    (void)data;
    (void)len;
    return Error::OK;
#endif
}

int RadiaCode::usbRead(uint8_t* buf, size_t max_len, uint32_t timeout_ms) {
#ifdef ARDUINO
    if (_in_xfer == nullptr || _dev_hdl == nullptr) {
        return -1;
    }

    usb_transfer_t* xfer = reinterpret_cast<usb_transfer_t*>(_in_xfer);
    xfer->num_bytes = max_len;

    esp_err_t ret = usb_host_transfer_submit(xfer);
    if (ret != ESP_OK) {
        return -1;
    }

    // Wait for transfer completion (semaphore is given in usbTransferCallback)
    xSemaphoreTake((SemaphoreHandle_t)_xfer_semaphore, pdMS_TO_TICKS(timeout_ms));

    int received = xfer->actual_num_bytes;
    if (received <= 0) {
        return 0;  // zero-length, caller decides retry
    }
    std::memcpy(buf, xfer->data_buffer, received);
    return received;
#else
    // Unit-test stub: return 0 (no hardware)
    (void)buf;
    (void)max_len;
    (void)timeout_ms;
    return 0;
#endif
}

void RadiaCode::drainStaleData() {
    // Read and discard data until we get a timeout (empty read).
    // The Python reference does: while True: try read(256, timeout=100) except TimeoutError: break
    uint8_t discard_buf[USB_PACKET_SIZE];
    for (int i = 0; i < DRAIN_MAX_PACKETS; i++) {
        int n = usbRead(discard_buf, sizeof(discard_buf), /*timeout_ms=*/100);
        if (n <= 0) {
            break;  // timeout or nothing left to drain
        }
    }
}

// ============================================================================
// Private: USB Host init / teardown
// ============================================================================

Error RadiaCode::initUsbHost() {
#ifdef ARDUINO
    // Install the USB Host library
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;

    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means already installed — that's OK
        return Error::CONNECT_FAILED;
    }

    // Register a USB Host client
    usb_host_client_config_t client_config = {};
    client_config.is_synchronous = true;
    client_config.max_num_event_msg = 5;
    client_config.async.client_event_callback = nullptr;
    client_config.async.callback_arg = nullptr;

    ret = usb_host_client_register(&client_config,
                                   reinterpret_cast<usb_host_client_handle_t*>(&_client_hdl));
    if (ret != ESP_OK) {
        usb_host_uninstall();
        return Error::CONNECT_FAILED;
    }

    // Create binary semaphore for USB transfer synchronization
    _xfer_semaphore = xSemaphoreCreateBinary();

    return Error::OK;
#else
    // Unit-test stub
    return Error::OK;
#endif
}

Error RadiaCode::findAndOpenDevice() {
#ifdef ARDUINO
    // Wait briefly for device enumeration
    vTaskDelay(pdMS_TO_TICKS(500));

    // Get the list of connected device addresses
    uint8_t dev_addr_list[10] = {0};
    int num_devs = 0;
    esp_err_t ret = usb_host_device_addr_list_fill(
        10, dev_addr_list, &num_devs);
    if (ret != ESP_OK || num_devs == 0) {
        return Error::DEVICE_NOT_FOUND;
    }

    for (int i = 0; i < num_devs; i++) {
        usb_device_handle_t tmp_hdl = nullptr;
        ret = usb_host_device_open(
            reinterpret_cast<usb_host_client_handle_t>(_client_hdl),
            dev_addr_list[i],
            reinterpret_cast<usb_device_handle_t*>(&tmp_hdl));
        if (ret != ESP_OK) continue;

        const usb_device_desc_t* dev_desc = nullptr;
        ret = usb_host_get_device_descriptor(
            reinterpret_cast<usb_device_handle_t>(tmp_hdl), &dev_desc);
        if (ret == ESP_OK && dev_desc) {
            if (dev_desc->idVendor == RadiaCode::VID &&
                dev_desc->idProduct == RadiaCode::PID) {
                _dev_hdl = tmp_hdl;
                return Error::OK;
            }
        }

        usb_host_device_close(
            reinterpret_cast<usb_host_client_handle_t>(_client_hdl),
            reinterpret_cast<usb_device_handle_t>(tmp_hdl));
    }
    return Error::DEVICE_NOT_FOUND;
#else
    return Error::DEVICE_NOT_FOUND;  // No hardware in tests
#endif
}

Error RadiaCode::claimInterface() {
#ifdef ARDUINO
    if (_dev_hdl == nullptr) {
        return Error::CONNECT_FAILED;
    }

    // Claim interface 0 (the single bulk interface on RadiaCode)
    esp_err_t ret = usb_host_interface_claim(
        reinterpret_cast<usb_host_client_handle_t>(_client_hdl),
        reinterpret_cast<usb_device_handle_t>(_dev_hdl),
        /*bInterfaceNumber=*/0,
        /*bAlternateSetting=*/0);
    if (ret != ESP_OK) {
        return Error::CONNECT_FAILED;
    }

    // Allocate bulk OUT transfer (endpoint 0x01)
    ret = usb_host_transfer_alloc(USB_PACKET_SIZE + 8, 0,
                                  reinterpret_cast<usb_transfer_t**>(&_out_xfer));
    if (ret != ESP_OK) {
        return Error::CONNECT_FAILED;
    }
    usb_transfer_t* out_xfer = reinterpret_cast<usb_transfer_t*>(_out_xfer);
    out_xfer->device_handle = reinterpret_cast<usb_device_handle_t>(_dev_hdl);
    out_xfer->bEndpointAddress = RadiaCode::EP_OUT;  // 0x01
    out_xfer->callback = usbTransferCallback;
    out_xfer->context = _xfer_semaphore;
    out_xfer->timeout_ms = USB_TIMEOUT_MS;

    // Allocate bulk IN transfer (endpoint 0x81)
    ret = usb_host_transfer_alloc(USB_PACKET_SIZE + 8, 0,
                                  reinterpret_cast<usb_transfer_t**>(&_in_xfer));
    if (ret != ESP_OK) {
        usb_host_transfer_free(reinterpret_cast<usb_transfer_t*>(_out_xfer));
        _out_xfer = nullptr;
        return Error::CONNECT_FAILED;
    }
    usb_transfer_t* in_xfer = reinterpret_cast<usb_transfer_t*>(_in_xfer);
    in_xfer->device_handle = reinterpret_cast<usb_device_handle_t>(_dev_hdl);
    in_xfer->bEndpointAddress = RadiaCode::EP_IN;  // 0x81
    in_xfer->callback = usbTransferCallback;
    in_xfer->context = _xfer_semaphore;
    in_xfer->timeout_ms = USB_TIMEOUT_MS;

    return Error::OK;
#else
    return Error::OK;
#endif
}

void RadiaCode::releaseUsbHost() {
#ifdef ARDUINO
    if (_out_xfer) {
        usb_host_transfer_free(reinterpret_cast<usb_transfer_t*>(_out_xfer));
        _out_xfer = nullptr;
    }
    if (_in_xfer) {
        usb_host_transfer_free(reinterpret_cast<usb_transfer_t*>(_in_xfer));
        _in_xfer = nullptr;
    }
    if (_dev_hdl) {
        usb_host_device_close(
            reinterpret_cast<usb_host_client_handle_t>(_client_hdl),
            reinterpret_cast<usb_device_handle_t>(_dev_hdl));
        _dev_hdl = nullptr;
    }
    if (_xfer_semaphore) {
        vSemaphoreDelete((SemaphoreHandle_t)_xfer_semaphore);
        _xfer_semaphore = nullptr;
    }
    if (_client_hdl) {
        usb_host_client_deregister(
            reinterpret_cast<usb_host_client_handle_t>(_client_hdl));
        _client_hdl = nullptr;
        usb_host_uninstall();
    }
#else
    _out_xfer = nullptr;
    _in_xfer = nullptr;
    _dev_hdl = nullptr;
    _client_hdl = nullptr;
#endif
}

} // namespace radiacode

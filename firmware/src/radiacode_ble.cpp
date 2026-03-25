#include "radiacode_ble.h"
#include "portal/debug_ws.h"
#include <cstring>

// =============================================================================
// RadiaLog Firmware - RadiaCode BLE Transport Implementation
// Same command/response protocol as USB, but over BLE GATT characteristic.
// =============================================================================

namespace radiacode {

// ============================================================================
// Static helpers (duplicated from RadiaCode USB — small footprint, avoids
// coupling to USB-specific class internals)
// ============================================================================

void RadiaCodeBLE::packLE32(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

uint32_t RadiaCodeBLE::unpackLE32(const uint8_t* buf) {
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

uint32_t RadiaCodeBLE::getCurrentTimeSec() {
#ifdef ARDUINO
    return static_cast<uint32_t>(time(nullptr));
#else
    return 1700000000UL;
#endif
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

RadiaCodeBLE::RadiaCodeBLE()
    : _connected(false)
    , _seq(0)
    , _base_time(0)
#ifdef ARDUINO
    , _client(nullptr)
    , _characteristic(nullptr)
    , _responseSemaphore(nullptr)
#endif
    , _expectedResponseLen(0)
    , _responseReady(false)
{
#ifdef ARDUINO
    _responseSemaphore = xSemaphoreCreateBinary();
#endif
}

RadiaCodeBLE::~RadiaCodeBLE() {
    disconnect();
#ifdef ARDUINO
    if (_responseSemaphore) {
        vSemaphoreDelete(_responseSemaphore);
        _responseSemaphore = nullptr;
    }
#endif
}

// ============================================================================
// BLE Stack Init (call once)
// ============================================================================

void RadiaCodeBLE::initBLE() {
#ifdef ARDUINO
    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init("RadiaLog");
        // Allow enough connections for multiple RadiaCodes
        NimBLEDevice::setMTU(256);
    }
#endif
}

// ============================================================================
// Connect / Disconnect
// ============================================================================

Error RadiaCodeBLE::connect(const String& macAddress) {
#ifdef ARDUINO
    if (_connected) {
        return Error::OK;
    }

    _macAddress = macAddress;

    debugWS.log(MOD_USB, LVL_INFO,
        "[BLE] Connecting to RadiaCode at " + macAddress + "...");

    // Create a NimBLE client
    _client = NimBLEDevice::createClient();
    if (!_client) {
        debugWS.log(MOD_USB, LVL_WARN, "[BLE] Failed to create client.");
        return Error::CONNECT_FAILED;
    }

    _client->setConnectTimeout(5);

    // Connect by MAC address
    NimBLEAddress addr(macAddress.c_str());
    if (!_client->connect(addr)) {
        debugWS.log(MOD_USB, LVL_WARN,
            "[BLE] Failed to connect to " + macAddress);
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return Error::CONNECT_FAILED;
    }

    // Discover the RadiaCode service
    NimBLERemoteService* service = _client->getService(SERVICE_UUID);
    if (!service) {
        debugWS.log(MOD_USB, LVL_WARN, "[BLE] RadiaCode service not found.");
        _client->disconnect();
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return Error::DEVICE_NOT_FOUND;
    }

    // Get the command/response characteristic
    _characteristic = service->getCharacteristic(CHAR_UUID);
    if (!_characteristic) {
        debugWS.log(MOD_USB, LVL_WARN, "[BLE] RadiaCode characteristic not found.");
        _client->disconnect();
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return Error::DEVICE_NOT_FOUND;
    }

    // Subscribe to notifications for receiving responses
    if (_characteristic->canNotify()) {
        // Use a lambda that captures this instance pointer
        _characteristic->subscribe(true,
            [this](NimBLERemoteCharacteristic* chr, uint8_t* data,
                   size_t length, bool isNotify) {
                this->onNotify(data, length);
            });
    }

    _connected = true;
    _seq = 0;

    debugWS.log(MOD_USB, LVL_INFO,
        "[BLE] Connected to RadiaCode at " + macAddress);

    return Error::OK;
#else
    return Error::CONNECT_FAILED;
#endif
}

void RadiaCodeBLE::disconnect() {
#ifdef ARDUINO
    if (_client) {
        if (_client->isConnected()) {
            _client->disconnect();
        }
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    _characteristic = nullptr;
#endif
    _connected = false;
}

// ============================================================================
// Notification handler — accumulates response bytes
// ============================================================================

void RadiaCodeBLE::onNotify(const uint8_t* data, size_t len) {
    _responseBuffer.insert(_responseBuffer.end(), data, data + len);

    // Check if we have the length prefix yet
    if (_responseBuffer.size() >= 4 && _expectedResponseLen == 0) {
        _expectedResponseLen = unpackLE32(_responseBuffer.data());
    }

    // Check if the full response has arrived
    if (_expectedResponseLen > 0 &&
        _responseBuffer.size() >= 4 + _expectedResponseLen) {
        _responseReady = true;
#ifdef ARDUINO
        xSemaphoreGiveFromISR(_responseSemaphore, nullptr);
#endif
    }
}

// ============================================================================
// Transport: execute command/response over BLE
// ============================================================================

Error RadiaCodeBLE::execute(const std::vector<uint8_t>& request,
                             std::vector<uint8_t>& response) {
#ifdef ARDUINO
    if (!_connected || !_characteristic) {
        return Error::NOT_CONNECTED;
    }

    // Check if still connected
    if (!_client->isConnected()) {
        _connected = false;
        return Error::NOT_CONNECTED;
    }

    // Clear response state
    _responseBuffer.clear();
    _expectedResponseLen = 0;
    _responseReady = false;

    // Write request to characteristic (NimBLE handles MTU chunking)
    if (!_characteristic->writeValue(request.data(), request.size(), true)) {
        return Error::WRITE_FAILED;
    }

    // Wait for notification response
    if (xSemaphoreTake(_responseSemaphore, pdMS_TO_TICKS(BLE_TIMEOUT_MS)) != pdTRUE) {
        return Error::READ_TIMEOUT;
    }

    if (!_responseReady || _responseBuffer.size() < 4) {
        return Error::READ_FAILED;
    }

    // Parse response: skip 4-byte length prefix
    uint32_t respLen = unpackLE32(_responseBuffer.data());
    if (_responseBuffer.size() < 4 + respLen) {
        return Error::READ_FAILED;
    }

    response.assign(_responseBuffer.begin() + 4,
                     _responseBuffer.begin() + 4 + respLen);
    return Error::OK;
#else
    return Error::NOT_CONNECTED;
#endif
}

// ============================================================================
// Protocol methods (same logic as USB RadiaCode class)
// ============================================================================

std::vector<uint8_t> RadiaCodeBLE::buildRequest(uint16_t command_id,
                                                  const std::vector<uint8_t>& args) {
    uint8_t seq_byte = static_cast<uint8_t>(0x80 + (_seq % 32));
    _seq = (_seq + 1) % 32;

    std::vector<uint8_t> payload;
    payload.reserve(4 + args.size());
    payload.push_back(static_cast<uint8_t>(command_id & 0xFF));
    payload.push_back(static_cast<uint8_t>((command_id >> 8) & 0xFF));
    payload.push_back(0x00);
    payload.push_back(seq_byte);
    payload.insert(payload.end(), args.begin(), args.end());

    std::vector<uint8_t> request(4 + payload.size());
    packLE32(request.data(), static_cast<uint32_t>(payload.size()));
    std::memcpy(request.data() + 4, payload.data(), payload.size());

    return request;
}

Error RadiaCodeBLE::sendCommand(COMMAND command_id,
                                 const std::vector<uint8_t>& args,
                                 std::vector<uint8_t>& response) {
    std::vector<uint8_t> request = buildRequest(
        static_cast<uint16_t>(command_id), args);
    return execute(request, response);
}

std::vector<uint8_t> RadiaCodeBLE::read_request(VS vs_id) {
    uint16_t id = static_cast<uint16_t>(vs_id);
    std::vector<uint8_t> args = {
        static_cast<uint8_t>(id & 0xFF),
        static_cast<uint8_t>((id >> 8) & 0xFF),
    };
    return buildRequest(static_cast<uint16_t>(COMMAND::RD_VIRT_STRING), args);
}

std::vector<uint8_t> RadiaCodeBLE::write_request(uint16_t vsfr_id, uint32_t value) {
    std::vector<uint8_t> args(6);
    args[0] = static_cast<uint8_t>(vsfr_id & 0xFF);
    args[1] = static_cast<uint8_t>((vsfr_id >> 8) & 0xFF);
    packLE32(&args[2], value);
    return buildRequest(static_cast<uint16_t>(COMMAND::WR_VIRT_SFR), args);
}

// ============================================================================
// Init sequence (identical to USB)
// ============================================================================

bool RadiaCodeBLE::init() {
    if (!_connected) {
        return false;
    }

    // Step 1: SET_EXCHANGE with payload 0x01 0xff 0x12 0xff
    {
        std::vector<uint8_t> args = { 0x01, 0xff, 0x12, 0xff };
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::SET_EXCHANGE, args, resp);
        if (err != Error::OK) {
            debugWS.log(MOD_USB, LVL_WARN,
                "[BLE:" + _macAddress + "] SET_EXCHANGE failed.");
            return false;
        }
    }

    // Step 2: SET_TIME
    {
        uint32_t now_sec = getCurrentTimeSec();
        std::vector<uint8_t> args(4);
        packLE32(args.data(), now_sec);
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::SET_TIME, args, resp);
        if (err != Error::OK) {
            debugWS.log(MOD_USB, LVL_WARN,
                "[BLE:" + _macAddress + "] SET_TIME failed.");
            return false;
        }
    }

    // Step 3: Write DEVICE_TIME to VSFR 0x0504 with value 0
    {
        std::vector<uint8_t> req = write_request(0x0504, 0);
        std::vector<uint8_t> resp;
        Error err = execute(req, resp);
        if (err != Error::OK) {
            debugWS.log(MOD_USB, LVL_WARN,
                "[BLE:" + _macAddress + "] DEVICE_TIME write failed.");
            return false;
        }
    }

    // Step 4: base_time = now() + 128 seconds
    _base_time = getCurrentTimeSec() + 128;

    // Step 5: Read firmware version
    {
        std::vector<uint8_t> resp;
        Error err = sendCommand(COMMAND::GET_VERSION, {}, resp);
        if (err == Error::OK && !resp.empty()) {
            char ver_str[64] = {0};
            size_t copy_len = resp.size() < sizeof(ver_str) - 1
                            ? resp.size() : sizeof(ver_str) - 1;
            memcpy(ver_str, resp.data(), copy_len);
            debugWS.log(MOD_USB, LVL_INFO,
                "[BLE:" + _macAddress + "] FW version: " + String(ver_str));
        }
    }

    debugWS.log(MOD_USB, LVL_INFO,
        "[BLE:" + _macAddress + "] Init sequence complete.");
    return true;
}

// ============================================================================
// Read and decode the data buffer
// ============================================================================

std::vector<RadiaCodeReading> RadiaCodeBLE::readDataBuf() {
    std::vector<uint8_t> request = read_request(VS::DATA_BUF);
    std::vector<uint8_t> response;

    Error err = execute(request, response);
    if (err != Error::OK || response.empty()) {
        // Check if we lost connection
        if (err == Error::NOT_CONNECTED) {
            _connected = false;
        }
        return {};
    }

    if (response.size() <= 4) {
        return {};
    }

    const uint8_t* data = response.data() + 4;
    size_t data_len = response.size() - 4;
    uint64_t base_time_ms = static_cast<uint64_t>(_base_time) * 1000ULL;

    // Reuse the static parser from the USB RadiaCode class
    return RadiaCode::parseDataBuf(data, data_len, base_time_ms);
}

} // namespace radiacode

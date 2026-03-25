// =============================================================================
// RadiaLog Firmware - WebSocket Debug Logger Implementation
// =============================================================================

#include "debug_ws.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Global singleton
// ---------------------------------------------------------------------------
DebugWS debugWS;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
DebugWS::DebugWS()
    : _ws(nullptr)
    , _head(0)
    , _count(0)
    , _minLevel(LVL_INFO)
{
}

// ---------------------------------------------------------------------------
// begin() — register /ws/debug on the server
// ---------------------------------------------------------------------------
void DebugWS::begin(AsyncWebServer* server) {
    _ws = new AsyncWebSocket("/ws/debug");

    _ws->onEvent([this](AsyncWebSocket* server,
                        AsyncWebSocketClient* client,
                        AwsEventType type,
                        void* arg,
                        uint8_t* data,
                        size_t len) {
        _onEvent(server, client, type, arg, data, len);
    });

    // Server may be nullptr during early init; portal registers the
    // WebSocket handler later via getSocket().
    if (server != nullptr) {
        server->addHandler(_ws);
        Serial.println("[DEBUG_WS] WebSocket registered at /ws/debug");
    } else {
        Serial.println("[DEBUG_WS] WebSocket created (server registration deferred)");
    }
}

// ---------------------------------------------------------------------------
// log() — write to circular buffer, Serial, and WS clients
// ---------------------------------------------------------------------------
void DebugWS::log(LogModule module, LogLevel level, const String& message) {
    uint32_t ts = millis();

    // Always print to Serial
    String serial_line = String("[") + _moduleStr(module) + "]["
                       + _levelStr(level) + "] " + message;
    Serial.println(serial_line);

    // Store in circular buffer (always, regardless of level filter)
    LogEntry entry;
    entry.module    = module;
    entry.level     = level;
    entry.message   = message;
    entry.timestamp = ts;

    _buffer[_head] = entry;
    _head = (_head + 1) % DEBUG_WS_BUFFER_SIZE;
    if (_count < DEBUG_WS_BUFFER_SIZE) {
        _count++;
    }

    // Broadcast to WebSocket clients if level passes filter
    if (_ws && level <= _minLevel) {
        String json = _formatEntry(entry);
        _ws->textAll(json);
    }
}

// ---------------------------------------------------------------------------
// setLevel() — configure minimum WS output level
// ---------------------------------------------------------------------------
void DebugWS::setLevel(LogLevel level) {
    _minLevel = level;
}

// ---------------------------------------------------------------------------
// getSocket()
// ---------------------------------------------------------------------------
AsyncWebSocket* DebugWS::getSocket() {
    return _ws;
}

// ---------------------------------------------------------------------------
// _onEvent() — WebSocket event handler
// ---------------------------------------------------------------------------
void DebugWS::_onEvent(AsyncWebSocket* server,
                       AsyncWebSocketClient* client,
                       AwsEventType type,
                       void* arg,
                       uint8_t* data,
                       size_t len)
{
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[DEBUG_WS] Client #%u connected from %s\n",
                      client->id(),
                      client->remoteIP().toString().c_str());

        // Replay circular buffer to this client (oldest first)
        if (_count == 0) return;

        // Calculate the index of the oldest entry
        uint16_t oldest;
        if (_count < DEBUG_WS_BUFFER_SIZE) {
            // Buffer not yet full: oldest entry is at index 0
            oldest = 0;
        } else {
            // Buffer full: oldest entry is at _head (next write position)
            oldest = _head;
        }

        for (uint16_t i = 0; i < _count; i++) {
            uint16_t idx = (oldest + i) % DEBUG_WS_BUFFER_SIZE;
            String json = _formatEntry(_buffer[idx]);
            client->text(json);
        }

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[DEBUG_WS] Client #%u disconnected\n", client->id());

    } else if (type == WS_EVT_ERROR) {
        Serial.printf("[DEBUG_WS] Client #%u error\n", client->id());
    }
}

// ---------------------------------------------------------------------------
// _moduleStr() — convert LogModule to string tag
// ---------------------------------------------------------------------------
const char* DebugWS::_moduleStr(LogModule module) const {
    switch (module) {
        case MOD_USB:    return "USB";
        case MOD_GPS:    return "GPS";
        case MOD_WIFI:   return "WIFI";
        case MOD_UPLOAD: return "UPLOAD";
        case MOD_BUFFER: return "BUFFER";
        case MOD_BLE:    return "BLE";
        default:         return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// _levelStr() — convert LogLevel to string
// ---------------------------------------------------------------------------
const char* DebugWS::_levelStr(LogLevel level) const {
    switch (level) {
        case LVL_ERROR: return "ERROR";
        case LVL_WARN:  return "WARN";
        case LVL_INFO:  return "INFO";
        case LVL_DEBUG: return "DEBUG";
        default:        return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// _formatEntry() — format a LogEntry as a JSON string
// ---------------------------------------------------------------------------
String DebugWS::_formatEntry(const LogEntry& entry) const {
    // Build a compact JSON object:
    // {"ts":12345,"module":"GPS","level":"INFO","msg":"..."}
    String json;
    json.reserve(128);
    json  = "{\"ts\":";
    json += entry.timestamp;
    json += ",\"module\":\"";
    json += _moduleStr(entry.module);
    json += "\",\"level\":\"";
    json += _levelStr(entry.level);
    json += "\",\"msg\":\"";

    // Escape double-quotes and backslashes in message
    for (size_t i = 0; i < entry.message.length(); i++) {
        char c = entry.message[i];
        if (c == '"' || c == '\\') json += '\\';
        json += c;
    }

    json += "\"}";
    return json;
}

// ---------------------------------------------------------------------------
// Global convenience free function
// ---------------------------------------------------------------------------
void dbgLog(LogModule module, LogLevel level, const String& message) {
    debugWS.log(module, level, message);
}

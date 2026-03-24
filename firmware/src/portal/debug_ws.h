#pragma once
#ifndef PORTAL_DEBUG_WS_H
#define PORTAL_DEBUG_WS_H

// =============================================================================
// RadiaLog Firmware - WebSocket Debug Logger
// Streams firmware log output to connected web clients via /ws/debug
// =============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// ---------------------------------------------------------------------------
// Log level enumeration (ordered: ERROR < WARN < INFO < DEBUG)
// ---------------------------------------------------------------------------
enum LogLevel {
    ERROR = 0,
    WARN  = 1,
    INFO  = 2,
    DEBUG = 3
};

// ---------------------------------------------------------------------------
// Log module tags
// ---------------------------------------------------------------------------
enum LogModule {
    USB    = 0,
    GPS    = 1,
    WIFI   = 2,
    UPLOAD = 3,
    BUFFER = 4
};

// ---------------------------------------------------------------------------
// Single log entry stored in the circular buffer
// ---------------------------------------------------------------------------
struct LogEntry {
    LogModule module;
    LogLevel  level;
    String    message;
    uint32_t  timestamp;  // millis() at time of log
};

// ---------------------------------------------------------------------------
// Circular buffer capacity
// ---------------------------------------------------------------------------
static const uint16_t DEBUG_WS_BUFFER_SIZE = 500;

// ---------------------------------------------------------------------------
// DebugWS class
// ---------------------------------------------------------------------------
class DebugWS {
public:
    DebugWS();

    /// Register /ws/debug on the given server and set up the event handler.
    void begin(AsyncWebServer* server);

    /// Log a message. Writes to Serial always; writes to WebSocket clients
    /// only if the message level >= _minLevel.
    void log(LogModule module, LogLevel level, const String& message);

    /// Set the minimum log level for WebSocket output.
    /// Messages below this level are still printed to Serial.
    void setLevel(LogLevel level);

    /// Direct access to the underlying AsyncWebSocket (used by portal).
    AsyncWebSocket* getSocket();

private:
    AsyncWebSocket* _ws;

    // Circular buffer
    LogEntry  _buffer[DEBUG_WS_BUFFER_SIZE];
    uint16_t  _head;      // index where next entry will be written
    uint16_t  _count;     // number of valid entries (0..DEBUG_WS_BUFFER_SIZE)

    LogLevel  _minLevel;  // minimum level forwarded to WebSocket clients

    // Internal helpers
    const char* _moduleStr(LogModule module) const;
    const char* _levelStr(LogLevel level) const;
    String      _formatEntry(const LogEntry& entry) const;

    void _onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                  AwsEventType type, void* arg, uint8_t* data, size_t len);
};

// ---------------------------------------------------------------------------
// Global singleton instance
// ---------------------------------------------------------------------------
extern DebugWS debugWS;

// ---------------------------------------------------------------------------
// Convenience free function — writes to Serial and all connected WS clients
// ---------------------------------------------------------------------------
void dbgLog(LogModule module, LogLevel level, const String& message);

#endif // PORTAL_DEBUG_WS_H

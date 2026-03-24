#include <Arduino.h>
#include "config.h"

// =============================================================================
// RadiaLog Firmware - Main Entry Point
// Board: Seeed XIAO ESP32S3
// =============================================================================
// Phase 1: USB Host communication with RadiaCode + GPS via UART
// Subsequent stories will add:
//   - radiacode.h/cpp  (USB Host protocol)
//   - gps/atgm336h.h/cpp (GPS UART driver)
//   - buffer.h/cpp (LittleFS reading storage)
//   - uploader.h/cpp (WiFi + HTTP batch upload)
//   - portal/ (Status web server)
// =============================================================================

void setup() {
    // Initialize serial debug output
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) {
        delay(10);
    }

    Serial.println(F("[RadiaLog] Firmware starting..."));
    Serial.printf("[RadiaLog] Board: XIAO ESP32S3\n");
    Serial.printf("[RadiaLog] Poll interval: %d ms\n", POLL_INTERVAL_MS);
    Serial.printf("[RadiaLog] RadiaCode VID:PID = 0x%04X:0x%04X\n",
                  RADIACODE_VID, RADIACODE_PID);
    Serial.printf("[RadiaLog] GPS TX:%d RX:%d BAUD:%d\n",
                  GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);

    // Initialize status LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // TODO (US-002): Initialize USB Host and connect to RadiaCode
    // TODO (US-003): Initialize GPS UART driver
    // TODO (US-004): Initialize LittleFS buffer
    // TODO (US-005): Initialize WiFi (AP + STA)
    // TODO (US-006): Initialize upload manager
    // TODO (US-007): Initialize status portal

    Serial.println(F("[RadiaLog] Setup complete (Phase 1 stub)"));
}

void loop() {
    // TODO (US-002): Poll RadiaCode USB for radiation readings
    // TODO (US-003): Poll GPS for position fix
    // TODO (US-004): Store merged reading to flash buffer
    // TODO (US-006): Trigger upload if WiFi connected

    delay(POLL_INTERVAL_MS);
}

#pragma once

// =============================================================================
// RadiaLog Firmware - Hardware Configuration
// Board-specific pin mappings selected via build flags:
//   -DBOARD_XIAO_ESP32S3   → Seeed XIAO ESP32S3 + Wio-SX1262
//   -DBOARD_TDISPLAY_S3    → LilyGO T-Display S3 (1.9" TFT)
// =============================================================================

// Firmware version
#define FW_VERSION      "0.1.0"

// =============================================================================
// Board: Seeed XIAO ESP32S3 + Wio-SX1262
// =============================================================================
#if defined(BOARD_XIAO_ESP32S3)

// GPS UART1
#define GPS_TX_PIN      43
#define GPS_RX_PIN      44
#define GPS_BAUD        9600
#define GPS_UART_NUM    1

// Status LED (built-in, GPIO21)
#define LED_PIN         21

// Battery ADC (external voltage divider on A0)
#define BATTERY_ADC_PIN       1
#define BATTERY_DIVIDER_RATIO 2.0f

// LoRa (Wio-SX1262) — reserved for future
#define LORA_SCK_PIN    8
#define LORA_MISO_PIN   9
#define LORA_MOSI_PIN   10
#define LORA_CS_PIN     41
#define LORA_RST_PIN    42
#define LORA_BUSY_PIN   40
#define LORA_DIO1_PIN   39

// =============================================================================
// Board: LilyGO T-Display S3 (170x320 ST7789)
// =============================================================================
#elif defined(BOARD_TDISPLAY_S3)

// GPS UART1 — uses GPIO12/13 (GPIO17/18 are wired to touch IC)
#define GPS_TX_PIN      12
#define GPS_RX_PIN      13
#define GPS_BAUD        9600
#define GPS_UART_NUM    1

// No user-addressable LED on T-Display S3
#define LED_PIN         -1

// Battery ADC (built-in divider on GPIO4)
#define BATTERY_ADC_PIN       4
#define BATTERY_DIVIDER_RATIO 2.0f

// Display
#define HAS_DISPLAY     1
#define TFT_BL_PIN      38
#define TFT_POWER_PIN   15

// User button
#define HAS_BUTTON      1
#define BUTTON_PIN      14

#else
#error "No board defined. Add -DBOARD_XIAO_ESP32S3 or -DBOARD_TDISPLAY_S3 to build_flags."
#endif

// =============================================================================
// Common settings (all boards)
// =============================================================================

// Timing
#define POLL_INTERVAL_MS    2000
#define GPS_TIMEOUT_MS      5000

// Battery thresholds (mV)
#define BATTERY_SAMPLES       16
#define BATTERY_FULL_MV       4200
#define BATTERY_EMPTY_MV      3000

// NTP
#define NTP_SERVER_1    "pool.ntp.org"
#define NTP_SERVER_2    "time.nist.gov"

// WiFi / Portal
#define AP_SSID_PREFIX      "RadiaLog"
#define AP_CHANNEL          6
#define PORTAL_IP           "192.168.4.1"

// Serial debug
#define SERIAL_BAUD         115200

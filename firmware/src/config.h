#pragma once

// =============================================================================
// RadiaLog Firmware - Hardware Configuration
// Board-specific pin mappings selected via build flags:
//   -DBOARD_XIAO_ESP32S3   → Seeed XIAO ESP32S3 + Wio-SX1262  (no display)
//   -DBOARD_TDISPLAY_S3    → LilyGO T-Display S3 (1.9" TFT)
//   -DBOARD_TDISPLAY_S3_AMOLED → LilyGO T-Display S3 AMOLED (1.91" RM67162)
//   -DBOARD_WS_AMOLED_175  → Waveshare ESP32-S3-Touch-AMOLED-1.75-G (circular)
//
// To add a new board:
//   1. Add a new #elif block below with pin mappings
//   2. Define HAS_DISPLAY, DISPLAY_WIDTH, DISPLAY_HEIGHT if it has a screen
//   3. For circular displays, also define DISPLAY_CIRCULAR 1
//   4. Add a matching [env:...] section in platformio.ini with the correct
//      build flags (-DBOARD_xxx) and display library dependencies
// =============================================================================

// Firmware version
#define FW_VERSION      "0.1.0"

// =============================================================================
// Board: Seeed XIAO ESP32S3 + Wio-SX1262  (no display)
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

// No display on this board

// =============================================================================
// Board: LilyGO T-Display S3 (170x320 ST7789, parallel 8-bit)
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

// Display — 170x320 ST7789 (portrait)
#define HAS_DISPLAY       1
#define DISPLAY_WIDTH     170
#define DISPLAY_HEIGHT    320
#define TFT_BL_PIN        38
#define TFT_POWER_PIN     15

// User buttons (T-Display S3 has two: GPIO14 and boot button GPIO0)
#define HAS_BUTTON      1
#define BUTTON_PIN      14
#define BUTTON2_PIN     0   // GPIO0 — shared with BOOT_BUTTON_PIN (common boot button)

// Touch (CST816S capacitive touch controller)
#define HAS_TOUCH       1
#define TOUCH_INT_PIN   16
#define TOUCH_RST_PIN   21

// =============================================================================
// Board: LilyGO T-Display S3 AMOLED (240x536 RM67162, QSPI)
// =============================================================================
#elif defined(BOARD_TDISPLAY_S3_AMOLED)

// GPS UART1
#define GPS_TX_PIN      12
#define GPS_RX_PIN      13
#define GPS_BAUD        9600
#define GPS_UART_NUM    1

#define LED_PIN         -1

// Battery ADC (built-in divider on GPIO4)
#define BATTERY_ADC_PIN       4
#define BATTERY_DIVIDER_RATIO 2.0f

// Display — 240x536 RM67162 AMOLED (portrait)
#define HAS_DISPLAY       1
#define DISPLAY_WIDTH     240
#define DISPLAY_HEIGHT    536
#define TFT_BL_PIN        38
#define TFT_POWER_PIN     15

// User buttons
#define HAS_BUTTON      1
#define BUTTON_PIN      21
#define BUTTON2_PIN     0   // GPIO0 — shared with BOOT_BUTTON_PIN (common boot button)

// Touch (CST816S)
#define HAS_TOUCH       1
#define TOUCH_INT_PIN   16
#define TOUCH_RST_PIN   17

// =============================================================================
// Board: Waveshare ESP32-S3 1.28" Round (240x240 GC9A01, SPI, circular)
// =============================================================================
#elif defined(BOARD_WAVESHARE_ROUND)

// GPS UART1
#define GPS_TX_PIN      17
#define GPS_RX_PIN      18
#define GPS_BAUD        9600
#define GPS_UART_NUM    1

#define LED_PIN         -1

// Battery ADC
#define BATTERY_ADC_PIN       1
#define BATTERY_DIVIDER_RATIO 2.0f

// Display — 240x240 GC9A01 circular
#define HAS_DISPLAY       1
#define DISPLAY_WIDTH     240
#define DISPLAY_HEIGHT    240
#define DISPLAY_CIRCULAR  1
#define TFT_BL_PIN        2
#define TFT_POWER_PIN     -1   // No separate power control

// Single button (boot button)
#define HAS_BUTTON      1
#define BUTTON_PIN      0
#define BUTTON2_PIN     -1

// =============================================================================
// Board: Waveshare ESP32-S3-Touch-AMOLED-1.75-G
//        466x466 circular AMOLED (CO5300 QSPI) + LC76G GPS (I2C)
// =============================================================================
#elif defined(BOARD_WS_AMOLED_175)

// GPS — Quectel LC76G via I2C (shared bus with touch, IMU, etc.)
#define GPS_TYPE_I2C      1
#define GPS_I2C_SDA       15
#define GPS_I2C_SCL       14
#define GPS_I2C_ADDR      0x10

#define LED_PIN           -1

// Battery — AXP2101 PMU via I2C (same bus as GPS)
#define BATTERY_TYPE_AXP2101  1
#define PMU_I2C_SDA       15
#define PMU_I2C_SCL       14

// Display — 466x466 CO5300 AMOLED (QSPI, circular)
#define HAS_DISPLAY         1
#define DISPLAY_WIDTH       466
#define DISPLAY_HEIGHT      466
#define DISPLAY_CIRCULAR    1
#define DISPLAY_DRIVER_LGFX 1
// QSPI pins: SCLK=38, D0=4, D1=5, D2=6, D3=7, CS=12, RST=39, BL=47
// (configured in display_gfx.h LovyanGFX class)
#define TFT_BL_PIN          47
#define TFT_POWER_PIN       -1   // Controlled by AXP2101

// Single button (boot button on GPIO0)
#define HAS_BUTTON        1
#define BUTTON_PIN        0
#define BUTTON2_PIN       -1

// Touch (CST9217 on shared I2C bus)
#define HAS_TOUCH         1
#define TOUCH_INT_PIN     11
#define TOUCH_RST_PIN     40

#else
#error "No board defined. Pass -DBOARD_XIAO_ESP32S3, -DBOARD_TDISPLAY_S3, -DBOARD_TDISPLAY_S3_AMOLED, -DBOARD_WAVESHARE_ROUND, or -DBOARD_WS_AMOLED_175 in build_flags."
#endif

// =============================================================================
// Common settings (all boards)
// =============================================================================

// Boot button (GPIO0) — available on all ESP32-S3 boards
// NOTE: On T-Display S3 and AMOLED, GPIO0 is also defined as BUTTON2_PIN above.
//       On XIAO (no HAS_BUTTON), this is the only button input.
#define BOOT_BUTTON_PIN             0

// Shipping mode — long-press BOOT_BUTTON_PIN to enter deep sleep
#define SHIPPING_MODE_HOLD_MS       5000   // ms to hold for shipping mode
#define SHIPPING_MODE_BLINK_COUNT   3      // LED confirmation blinks before sleep
#define SHIPPING_MODE_BLINK_MS      200    // LED blink on/off duration (ms)

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

#pragma once

// =============================================================================
// RadiaLog Firmware - Hardware Configuration
// Board: Seeed XIAO ESP32S3 + Wio-SX1262
// =============================================================================

// -----------------------------------------------------------------------------
// GPS Module - ATGM336H UART (Hardware UART1)
// Connected to XIAO ESP32S3 GPIO pins D6 (TX) / D7 (RX)
// -----------------------------------------------------------------------------
#define GPS_TX_PIN      43      // XIAO ESP32S3 D6 → GPS RX
#define GPS_RX_PIN      44      // XIAO ESP32S3 D7 → GPS TX
#define GPS_BAUD        9600    // ATGM336H default baud rate
#define GPS_UART_NUM    1       // Hardware UART1

// -----------------------------------------------------------------------------
// USB Host - RadiaCode radiation detector
// XIAO ESP32S3 uses native USB (GPIO 19/20) in OTG/Host mode
// Built with -DARDUINO_USB_MODE=0
// -----------------------------------------------------------------------------
#define USB_DP_PIN      20      // USB D+ (native, fixed)
#define USB_DM_PIN      19      // USB D- (native, fixed)

// RadiaCode USB device identifiers
#define RADIACODE_VID   0x0483  // STMicroelectronics
#define RADIACODE_PID   0xF123  // RadiaCode firmware

// RadiaCode USB bulk endpoints
#define USB_WRITE_EP    0x01    // Bulk OUT endpoint (host → device)
#define USB_READ_EP     0x81    // Bulk IN endpoint (device → host)

// -----------------------------------------------------------------------------
// Status LED
// XIAO ESP32S3 has a built-in LED on GPIO21
// -----------------------------------------------------------------------------
#define LED_PIN         21      // Built-in LED (active HIGH)

// -----------------------------------------------------------------------------
// LoRa (Wio-SX1262) - Reserved for future use
// SPI interface on XIAO ESP32S3
// -----------------------------------------------------------------------------
#define LORA_SCK_PIN    8
#define LORA_MISO_PIN   9
#define LORA_MOSI_PIN   10
#define LORA_CS_PIN     41
#define LORA_RST_PIN    42
#define LORA_BUSY_PIN   40
#define LORA_DIO1_PIN   39

// -----------------------------------------------------------------------------
// Timing / Polling
// -----------------------------------------------------------------------------
#define POLL_INTERVAL_MS    2000    // Read RadiaCode + GPS every 2 seconds
#define GPS_TIMEOUT_MS      5000    // GPS timeout per reading cycle
#define USB_TIMEOUT_MS      3000    // USB read timeout

// -----------------------------------------------------------------------------
// WiFi / Portal defaults
// -----------------------------------------------------------------------------
#define AP_SSID_PREFIX      "RadiaLog"
#define AP_CHANNEL          6
#define PORTAL_IP           "192.168.4.1"

// -----------------------------------------------------------------------------
// Serial debug (via FTDI adapter on TX/RX pads during development)
// -----------------------------------------------------------------------------
#define SERIAL_BAUD         115200

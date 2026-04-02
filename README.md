# RadiaLog

**Standalone ESP32-S3 datalogger for RadiaCode radiation detectors with GPS and WiFi.**

Connect to a RadiaCode via USB or Bluetooth and RadiaLog automatically logs dose rate and count rate paired with GPS coordinates, then uploads everything to [RadiaMaps.com](https://radiamaps.com) over WiFi. No phone app required.

![RadiaLog Device](docs/images/radialog-device.jpg)
<!-- TODO: Add device photos -->

![RadiaLog Assembled](docs/images/radialog-assembled.jpg)
<!-- TODO: Add assembled device photo -->

## Features

- **Automatic logging** - Polls RadiaCode every ~2s via USB Host, records dose rate (uSv/h) + count rate (CPS) with GPS coordinates
- **Power-fail safe** - Readings stored in LittleFS flash buffer; survives unplugging at any time
- **WiFi dual-mode** - Hosts local `RadiaLog-XXXX` access point while connecting to WiFi for cloud uploads
- **Web portal** - Dashboard, settings, data export (CSV), and live debug console at `192.168.4.1`
- **Batch upload** - Sends up to 250 readings per request to RadiaMaps with exponential backoff retry
- **BLE support** - Connect up to 4 additional RadiaCode devices via Bluetooth simultaneously
- **OTA updates** - After initial USB flash, update firmware through the web portal
- **Multi-board** - Supports 5 ESP32-S3 boards with various displays

## Hardware

### Recommended Build (Compact / No Display)

| Component | Link |
|-----------|------|
| Seeed XIAO ESP32S3 | [Buy on Seeed Studio](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html) |
| ATGM336H GPS Module | [Buy on Amazon](https://www.amazon.com/dp/B09LQDG1PQ) |
| RadiaCode-10x | [radiacode.com](https://radiacode.com) |

### 3D Printable Case

[Download on MakerWorld](https://makerworld.com/en/models/XXXXXX)
<!-- TODO: Add MakerWorld link when published -->

### Supported Board

The **Seeed XIAO ESP32S3** is the only officially supported board. The recommended build above is the tested, supported configuration.

### Other Boards (Here Be Dragons)

Firmware builds exist for the boards below, but they're just for fun. They might work, they might not. No guarantees, no support.

| Board | Display | Build Target |
|-------|---------|--------------|
| LilyGO T-Display S3 | 1.9" TFT | `lilygo_tdisplay_s3` |
| LilyGO T-Display S3 AMOLED | 1.91" AMOLED | `lilygo_tdisplay_s3_amoled` |
| Waveshare ESP32-S3 Round | 1.28" circular | `waveshare_round` |
| Waveshare AMOLED 1.75-G | 1.75" circular AMOLED | `waveshare_amoled_175` |

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/install/cli) CLI
- USB cable for initial flash

### Build & Flash

```bash
cd firmware

# Build for your board
pio run -e seeed_xiao_esp32s3

# Flash via USB
pio run -e seeed_xiao_esp32s3 --target upload

# Monitor serial output
pio device monitor -b 115200
```

### Setup

1. Flash firmware via USB
2. Connect your phone/laptop to the `RadiaLog-XXXX` WiFi network
3. Open `http://192.168.4.1` in a browser
4. Configure your home WiFi and RadiaMaps token in Settings
5. Plug RadiaLog into your RadiaCode's USB-C port - logging starts automatically

## Web Portal

The portal is always accessible at `http://192.168.4.1` on the RadiaLog access point:

- **Dashboard** - Live dose rate, GPS status, WiFi, buffer depth, upload stats
- **Settings** - WiFi networks, RadiaMaps token, device name, reading interval
- **Data** - Recent readings table, mini chart, CSV export
- **Debug** - Live log streaming via WebSocket with module filters

## Architecture

```
RadiaCode (USB) ──> ESP32-S3 ──> LittleFS Buffer ──> WiFi Upload ──> RadiaMaps.com
                       |
GPS Module ────────────┘
                       |
              Web Portal (AP)
```

Readings are stored in a power-fail safe LittleFS buffer (~55K-110K readings capacity). A FreeRTOS task on core 0 handles batch uploads to RadiaMaps when WiFi is available. The web portal runs simultaneously on the device's access point.

## Project Structure

```
firmware/
├── platformio.ini          # Build config for all boards
└── src/
    ├── main.cpp            # Entry point
    ├── config.h            # Per-board pin definitions
    ├── radiacode.h/cpp     # USB Host protocol
    ├── radiacode_mgr.h/cpp # Multi-device manager (USB + BLE)
    ├── gps/                # GPS drivers (ATGM336H, LC76G)
    ├── buffer.h/cpp        # LittleFS reading storage
    ├── uploader.h/cpp      # Cloud upload manager
    ├── wifi_mgr.h/cpp      # AP + STA WiFi
    ├── portal/             # Web server + HTML pages
    ├── config_mgr.h/cpp    # JSON config persistence
    ├── battery.h/cpp       # Battery monitoring
    └── led.h/cpp           # Status LED patterns
```

## License

All rights reserved.

# RadiaLog

> **Don't want to build it yourself?** Pre-assembled RadiaLog devices are available at [RadiaMaps.com](https://radiamaps.com).

**Standalone ESP32-S3 datalogger for RadiaCode radiation detectors with GPS and WiFi.**

Connect to a RadiaCode via Bluetooth and RadiaLog automatically logs dose rate and count rate paired with GPS coordinates, then uploads everything to [RadiaMaps.com](https://radiamaps.com) over WiFi. No phone app required — configure everything through the built-in web portal.

[View the 3D printed case and photos on MakerWorld](https://makerworld.com/en/models/2638033-radialog-radiacode-mapping-companion#profileId-2914169)

## What It Does

- Pairs with your RadiaCode-10x via Bluetooth (up to 4 simultaneously)
- Reads dose rate (uSv/h) and count rate (CPS) every 2 seconds
- Tags each reading with GPS coordinates, altitude, speed, and heading
- Stores readings in flash — survives power loss, never loses data
- Uploads batches to [RadiaMaps.com](https://radiamaps.com) when WiFi is available
- Hosts its own WiFi network with a web dashboard for live stats and configuration

---

## Hardware You Need

### Recommended Build

| Component | Purpose | Approx. Cost |
|-----------|---------|--------------|
| [Seeed XIAO ESP32S3 Plus](https://www.seeedstudio.com/XIAO-ESP32S3-Plus-p-6361.html) | Brain of the device | ~$10 |
| [ATGM336H GPS Module](https://www.amazon.com/dp/B09LQDG1PQ) | GPS positioning | ~$8 |
| [RadiaCode-10x](https://radiacode.com) | Radiation detector | ~$300 |
| LiPo Battery (3.7V) | Portable power | ~$5 |
| 2x 200k ohm Resistors | Battery voltage sensing | ~$0.10 |

**Total (excluding RadiaCode): ~$25**

### Wiring

```
XIAO ESP32S3 Plus          ATGM336H GPS
─────────────────          ────────────
GPIO43 (TX)  ──────────>   RX
GPIO44 (RX)  <──────────   TX
3V3          ──────────>   VCC
GND          ──────────>   GND
```

### Battery Voltage Divider

The XIAO ESP32S3 Plus doesn't have a built-in battery voltage sense circuit. Solder two 200k ohm resistors in series between BAT+ and BAT- (which is GND). Tap the midpoint to GPIO9 (labeled D10/A10 on the board).

```
BAT+ ── R1 (200k) ──┬── R2 (200k) ── BAT-
                     │
                   GPIO9 (D10/A10)
```

This halves the battery voltage (4.2V becomes 2.1V) so it fits within the ESP32's ADC range. The high resistance keeps battery drain negligible (~10uA).

**Tip:** BAT- is the same as GND, so you can run the divider directly between the battery terminals. One wire from BAT+ through both resistors to BAT-, with the midpoint going to D10. This saves space in a tight enclosure.

### 3D Printable Case

[Download on MakerWorld](https://makerworld.com/en/models/2638033-radialog-radiacode-mapping-companion#profileId-2914169)

---

## Flashing the Firmware

### What You Need

- [PlatformIO](https://platformio.org/install/cli) (CLI or VS Code extension)
- USB-C cable

### Build and Flash

```bash
cd firmware

# Build
pio run -e seeed_xiao_esp32s3_plus

# Flash via USB
pio run -e seeed_xiao_esp32s3_plus --target upload

# (Optional) Watch serial output
pio device monitor -b 115200
```

### OTA Updates

After the first USB flash, you can update firmware over-the-air through the web portal. No need to plug in again.

---

## First-Time Setup

1. **Power on** the RadiaLog
2. **Connect** your phone or laptop to the `RadiaLog-XXXX` WiFi network (open network, no password)
3. **Open** `http://10.0.0.1` in a browser — the dashboard loads
4. **Go to Settings** and configure:
   - Your home/mobile WiFi network (up to 3 networks)
   - Your [RadiaMaps](https://radiamaps.com) device token (get one from your RadiaMaps account)
   - (Optional) Device name, reading interval, AP password
5. **Turn on** your RadiaCode — RadiaLog finds it via Bluetooth and starts logging automatically

That's it. Readings accumulate in the buffer and upload to RadiaMaps whenever WiFi is available.

---

## Web Portal

The portal is accessible at `http://10.0.0.1` when connected to the RadiaLog's AP, or via mDNS at `http://radialog.local` when on the same WiFi network.

### Dashboard

Live overview of everything happening on the device:

- **Radiation** — dose rate (uSv/h) and count rate (CPS)
- **GPS** — fix status, satellite count, coordinates, mini map
- **WiFi** — connection status, signal strength, SSID
- **RadiaCode** — USB/BLE connection status
- **Buffer** — readings stored, pending upload, last/next upload time
- **Power** — battery voltage/percentage, uptime, time sync source
- **System** — CPU usage, loop time, free heap, disk space, GPS health
- **RadiaMaps Account** — username, subscription, lifetime readings

### Settings

- WiFi networks (up to 3, priority ordered)
- RadiaMaps device token
- Device name
- Reading interval (default: 2 seconds)
- AP password (default: open)
- BLE device MAC addresses (for connecting specific RadiaCodes)
- Display timeout and button wake (for boards with screens)

### Data

- Recent readings table (last 100)
- CSV export of entire buffer
- Mini dose rate chart

### Debug Console

- Live log streaming via WebSocket
- Filter by module: USB, GPS, WiFi, Upload, Buffer
- Filter by level: Error, Warn, Info, Debug

### Actions

- **Force Upload** — trigger immediate upload to RadiaMaps
- **Clear Buffer** — wipe all stored readings
- **Reboot** — restart the device
- **OTA Update** — upload new firmware (.bin file)
- **Shutdown** — enter deep sleep (shipping mode)

---

## How It Works

```
RadiaCode ──(BLE)──> ESP32-S3 ──> LittleFS Buffer ──(WiFi)──> RadiaMaps.com
                        |
GPS Module ─────────────┘
                        |
                Web Portal (AP)
```

### Reading Flow

1. RadiaLog polls the RadiaCode every 2 seconds via Bluetooth
2. GPS provides coordinates (falls back to stored position or WiFi geolocation)
3. Each reading (34 bytes: lat, lon, dose, count, timestamp, altitude, speed, heading, accuracy) is appended to flash
4. A background task checks once per minute if it's time to upload
5. Uploads happen daily (with random jitter) or immediately when WiFi connects after being offline for 24+ hours
6. Readings stay in the buffer until the server confirms receipt — nothing is deleted on faith

### Power Management

- **CPU runs at 80MHz** (down from 240MHz) — all peripherals use hardware clock dividers, unaffected
- **Light sleep** kicks in during idle periods between readings (ESP-IDF automatic frequency scaling)
- **WiFi AP auto-disables** after 5 minutes with no connected clients — saves 40-60mA. Hit the reset button to bring it back.
- **WIFI_PS_MIN_MODEM** enabled — radio sleeps between DTIM beacons in STA mode
- **Shipping mode** — hold the boot button for 5 seconds for deep sleep (negligible power draw)

### Buffer Resilience

Readings are stored in LittleFS flash across three files: binary reading data, per-reading upload status, and an index file. If power cuts mid-write, the device reconciles on boot by cross-checking actual file sizes against the index — you lose at most one reading. The buffer holds ~55,000 readings per MB of flash (over 30 hours at 2-second intervals on an 8MB board).

---

## Status LED

| Pattern | Meaning |
|---------|---------|
| Solid | Everything connected and working |
| Slow blink (1s) | Waiting for GPS fix |
| Fast blink | Upload in progress |
| Double flash | No RadiaCode connected |
| Triple flash | Entering shipping mode (deep sleep) |

---

## Multiple RadiaCode Devices

RadiaLog supports up to 4 RadiaCode detectors via Bluetooth simultaneously. Add their MAC addresses in Settings. Each device's readings are logged independently with a device ID tag.

To find your RadiaCode's MAC address, use the BLE scan feature in the web portal's Settings page.

---

## RadiaMaps

[RadiaMaps.com](https://radiamaps.com) is the cloud platform that receives and visualizes your radiation mapping data. Create a free account, generate a device token, and paste it into the RadiaLog settings.

RadiaLog uploads readings in batches of up to 250 per request. Each reading includes GPS coordinates, dose rate, count rate, altitude, speed, and heading. Upload happens automatically — just keep your RadiaLog powered on near WiFi.

---

## Troubleshooting

### RadiaCode won't connect via Bluetooth
- Make sure your RadiaCode is powered on and not connected to the RadiaCode app on your phone (it can only connect to one device at a time)
- Check the LED — double flash means no RadiaCode found
- If using specific MAC addresses in settings, verify they're correct via the BLE scan feature

### No GPS fix
- GPS needs a clear view of the sky — it won't work indoors
- First fix (cold start) can take 1-5 minutes. Subsequent fixes are faster thanks to A-GPS
- The LED blinks slowly while waiting for GPS fix

### Readings not uploading
- Check that your WiFi credentials are correct in Settings
- Verify your RadiaMaps device token is valid (dashboard shows account info when connected)
- The debug console shows upload attempts and any errors
- Uploads are scheduled once daily — use "Force Upload" in the portal for immediate upload

### Can't connect to RadiaLog WiFi
- The AP auto-disables after 5 minutes with no clients to save power
- Press the **reset button** on the board to restart and bring the AP back
- Default SSID is `RadiaLog-XXXX` (last 4 hex digits of the board's MAC address)

### Battery reading shows N/A
- The XIAO ESP32S3 Plus requires an external voltage divider (two 200k resistors) — see [wiring section](#battery-voltage-divider)
- Make sure the divider midpoint connects to GPIO9 (D10/A10), not GPIO10

---

## Project Structure

```
firmware/
├── platformio.ini              # Build config (6 board targets)
├── sdkconfig.defaults          # ESP-IDF power management options
└── src/
    ├── main.cpp                # Setup + main loop
    ├── config.h                # Per-board pin definitions & constants
    ├── config_mgr.h/cpp        # Persistent JSON config (LittleFS + NVS)
    ├── buffer.h/cpp            # LittleFS-backed reading buffer (34 bytes/reading)
    ├── reading.h               # Reading struct definition
    ├── radiacode.h/cpp         # RadiaCode USB Host protocol
    ├── radiacode_ble.h/cpp     # RadiaCode BLE protocol
    ├── radiacode_mgr.h/cpp     # Multi-device manager (1 USB + 4 BLE)
    ├── gps/
    │   ├── gps.h               # Abstract GPS interface
    │   ├── atgm336h.h/cpp      # ATGM336H driver (UART, 9600 baud)
    │   └── lc76g_i2c.h/cpp     # LC76G driver (I2C, Waveshare board)
    ├── location_provider.h/cpp # GPS -> stored position -> WiFi geolocation fallback
    ├── uploader.h/cpp          # Background upload task (FreeRTOS, core 0)
    ├── wifi_mgr.h/cpp          # AP + STA dual mode, auto-reconnect, AP auto-off
    ├── battery.h/cpp           # ADC voltage monitor (16x oversampled)
    ├── battery_axp2101.h/cpp   # AXP2101 PMU driver (Waveshare AMOLED)
    ├── led.h/cpp               # Status LED patterns
    ├── display.h/cpp           # Display driver (TFT_eSPI / LovyanGFX)
    ├── shipping_mode.h/cpp     # Long-press deep sleep
    └── portal/
        ├── portal.h/cpp        # Async web server + API routes
        ├── debug_ws.h/cpp      # WebSocket debug console
        └── html/               # Dashboard, settings, data, debug pages
```

---

## Assembly

Don't have the tools or time to build one yourself? Pre-assembled RadiaLog devices are available for purchase at [RadiaMaps.com](https://radiamaps.com). Each unit is hand-assembled and tested in the USA.

**What you get:**
- Fully assembled RadiaLog (Seeed XIAO ESP32S3 Plus + ATGM336H GPS + battery)
- Firmware pre-flashed and ready to go
- 3D printed case included
- Battery voltage divider pre-soldered

---

## License

All rights reserved.

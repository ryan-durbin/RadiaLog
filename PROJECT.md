# RadiaLog

**Standalone ESP32 USB datalogger for RadiaCode radiation detectors with GPS and WiFi upload to RadiaMaps.com**

## Concept

Plug an ESP32 into a RadiaCode detector's USB-C port. The ESP32 continuously logs radiation readings (dose rate, count rate) along with GPS coordinates from a GPS module. The ESP32 runs in dual WiFi mode: connected to a network for uploads AND hosting a local AP portal for monitoring and configuration.

No phone needed. No app. Just power, detect, log, upload.

## Hardware

| Component | Role | Interface |
|-----------|------|-----------|
| **XIAO ESP32S3** | Brain. USB Host + WiFi + UART + I2C | — |
| **Wio-SX1262 carrier** | Carrier board (LoRa reserved for future) | SPI (to ESP32S3) |
| **RadiaCode-10x** | Radiation detector (dose rate, count rate) | **USB** (Host mode) |
| **ATGM336H GPS** | Lat/lon/altitude/speed/heading (v1) | UART (9600 baud) |
| **Battery / LiPo** | Portable power (Wio-SX1262 has charge management) | BAT pins |

### Board: XIAO ESP32S3 + Wio-SX1262

Chosen for compactness (21 x 18mm). ESP32-S3R8 dual-core 240MHz, 8MB PSRAM, WiFi + BLE 5.0, built-in LiPo charge management, I2C/UART/GPIO breakout. The Wio-SX1262 adds LoRa (reserved for future mesh/long-range features).

### USB Host: Single Port, No Problem

The XIAO ESP32S3's single USB-C port is dedicated to USB Host (RadiaCode connection). For programming/debug during development, use FTDI/USB-to-UART adapter on the XIAO's TX/RX pads. After initial flash, OTA updates via the WiFi portal.

**Live debug output is also available on the WiFi portal** (see Status Portal below), so once the firmware is flashed, you rarely need the UART adapter.

**Power consideration:** USB Host mode typically provides 5V VBUS. The RadiaCode has its own battery, but enumeration may require VBUS. Need to test whether RadiaCode enumerates without VBUS from the host side.

### GPS: ATGM336H (v1), Multi-Module Support (future)

The ATGM336H is a UART GPS module (9600 baud, GPS + BDS). Connected to ESP32S3's hardware UART pins (TX/RX).

**GPS abstraction layer:** The firmware will use a GPS interface/driver pattern so additional modules can be supported later (u-blox NEO-M8N, BN-880, etc.) by implementing the same interface. For v1, the ATGM336H UART driver is the only implementation.

### Why USB, Not Bluetooth

The ESP32 connects to the RadiaCode via **USB Host mode**, not Bluetooth. This keeps the RadiaCode's BLE free for the phone app. One wire, two jobs: data + power (if RadiaCode accepts charge over USB).

### RadiaCode USB Protocol

Fully documented in `docs/reference/radiacode_protocol.md`, reverse-engineered from [cdump/radiacode](https://github.com/cdump/radiacode).

Key facts:
- **VID:** `0x0483` (STMicroelectronics), **PID:** `0xF123`
- **Endpoints:** Write to `0x01`, read from `0x81` (bulk transfers)
- **Protocol:** Length-prefixed request/response. 4-byte LE uint32 length + payload.
- **Init sequence:** Drain stale data → SET_EXCHANGE → SET_TIME → DEVICE_TIME(0)
- **Data acquisition:** Read `VS.DATA_BUF` (0x100) → decode record stream → extract `RealTimeData` (dose_rate, count_rate)
- **Requires firmware >= 4.8**

## Software Architecture

```
┌───────────────────────────────────────────────────────┐
│                    XIAO ESP32S3                        │
│                                                       │
│  ┌──────────┐  ┌──────────┐  ┌─────────────────────┐ │
│  │ RadiaCode│  │   GPS    │  │    WiFi (AP+STA)    │ │
│  │  (USB)   │  │  (UART)  │  │  STA = upload link  │ │
│  └────┬─────┘  └────┬─────┘  │  AP  = local portal │ │
│       │              │        └───┬─────────────┬───┘ │
│       v              v            │             │     │
│  ┌────────────────────────┐       │             │     │
│  │    Reading Buffer      │       │             │     │
│  │    (LittleFS flash)    │◄──────┘             │     │
│  └────────────┬───────────┘                     │     │
│               │                                 │     │
│               v                                 v     │
│  ┌─────────────────────┐   ┌──────────────────────┐  │
│  │   Upload Manager    │   │   Status Portal      │  │
│  │   POST /api/        │   │   Dashboard + Config  │  │
│  │   radialog/upload   │   │   Debug Console       │  │
│  └─────────────────────┘   └──────────────────────┘  │
└───────────────────────────────────────────────────────┘
```

### WiFi: Dual Mode (AP + STA Simultaneously)

The ESP32 runs in `WIFI_AP_STA` mode: it connects to a configured WiFi network (STA) for RadiaMaps uploads while simultaneously broadcasting its own AP for the status portal. **Both are always active.**

- **STA (Station):** Connects to home WiFi / phone hotspot for internet access. Uploads happen here.
- **AP (Access Point):** `RadiaLog-XXXX` network (XXXX = last 4 of MAC). Portal at `192.168.4.1`. Always available, even when STA is disconnected or no WiFi configured.

### Core Modules

1. **RadiaCode USB Host** — ESP32-S3 USB Host mode. C++ port of cdump/radiacode protocol. Init device, poll `DATA_BUF` (VS 0x100) every ~2 seconds, decode `RealTimeData` records (dose_rate, count_rate). See `docs/reference/radiacode_protocol.md`.
2. **GPS Driver** — Abstracted GPS interface. v1 implementation: ATGM336H via UART (9600 baud, NMEA parsing via TinyGPSPlus). Future: add u-blox I2C driver, etc.
3. **Reading Buffer** — Store readings in LittleFS (flash) as compact binary (~34 bytes/reading). **Power-fail safe:** LittleFS uses copy-on-write; power loss mid-write loses at most 1 reading, everything else is intact. Device can be powered off and unplugged at any time without losing buffered data. Capacity: ~55K-110K readings (2-4MB partition), roughly 30-60 hours at 2-second intervals.
4. **WiFi Manager** — AP+STA dual mode. Auto-connect STA to configured networks. AP always on.
5. **Upload Manager** — When STA connected, batch-upload unsent readings in chunks of up to 10,000. **Upload loop:** send batch → wait for `success: true` → mark those readings as uploaded → send next batch → repeat until buffer is drained. **Critical:** readings are NOT removed from the buffer until a success response is confirmed. On failure/timeout, retry the same batch. Exponential backoff on repeated failures.
6. **Status Portal** — Web server on AP interface. Dashboard, config, debug console, data viewer.
7. **Status LED** — LED feedback: GPS fix, USB connected, WiFi STA connected, uploading, error states.

### Data Flow

1. Every ~2 seconds: read RadiaCode via USB → get dose_rate + count_rate
2. Every ~2 seconds: read GPS via UART → get lat, lon, altitude, speed, heading, accuracy
3. Merge into a reading record with timestamp
4. Append to flash buffer (power-fail safe, survives unplugging)
5. On WiFi STA connect: upload loop starts
   - a. Collect up to 10,000 unsent readings from buffer
   - b. POST to `/api/radialog/upload`
   - c. Wait for `success: true` response
   - d. Mark those specific readings as uploaded
   - e. If more unsent readings remain, go to (a)
   - f. If upload fails, retry with exponential backoff
6. Logging continues uninterrupted during uploads (separate task/core)

### Buffer Storage Capacity

| Partition Size | Readings | Duration @ 2s interval |
|----------------|----------|----------------------|
| 2MB | ~55,000 | ~30 hours |
| 3MB | ~82,000 | ~45 hours |
| 4MB | ~110,000 | ~61 hours |

Each reading is ~34 bytes in compact binary format. When the buffer approaches capacity, oldest uploaded (synced) readings are pruned first. Unuploaded readings are never pruned (they're the whole point).

### Reading Record Format

```json
{
  "latitude": 61.2181,
  "longitude": -149.9003,
  "dose_rate": 0.12,
  "count_rate": 3.5,
  "reading_time": "2026-03-24T02:00:00Z",
  "accuracy": 2.5,
  "altitude": 35.0,
  "speed_mph": 0.0,
  "speed_kph": 0.0,
  "heading": 180.0,
  "altitude_accuracy": 5.0
}
```

## RadiaMaps API Integration

### Dedicated RadiaLog Endpoint

RadiaLog gets its own route on RadiaMaps, separate from the web upload/live tracking endpoints.

### Endpoints

**`POST /api/radialog/upload`** — Batch upload readings (up to 10,000 per request)

```http
POST /api/radialog/upload
Content-Type: application/json
X-Device-Token: <device_auth_token>
X-Device-Id: RadiaLog-A1B2

{
  "readings": [
    {
      "latitude": 61.2181,
      "longitude": -149.9003,
      "dose_rate": 0.12,
      "count_rate": 3.5,
      "reading_time": "2026-03-24T02:00:00Z",
      "accuracy": 2.5,
      "altitude": 35.0,
      "speed_mph": 0.0,
      "speed_kph": 0.0,
      "heading": 180.0,
      "altitude_accuracy": 5.0
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "accepted": 150,
  "message": "150 readings ingested"
}
```

The 10,000 limit is per-request, not a cap on total data. The ESP32 upload manager handles chunking: send up to 10K readings, wait for `"success": true`, mark those as uploaded, then send the next batch. Repeat until buffer is drained. **No readings are removed from the buffer until a success response is confirmed.**

**`POST /api/radialog/verify`** — Verify device token and retrieve user info

```http
POST /api/radialog/verify
Content-Type: application/json
X-Device-Token: <device_auth_token>
```

**Response (success):**
```json
{
  "success": true,
  "user": {
    "username": "rymn_rd",
    "nickname": "Ryan",
    "user_id": 42,
    "member_since": "2025-06-15T00:00:00Z",
    "total_readings": 158432,
    "subscription_status": "premium"
  }
}
```

**Response (failure):**
```json
{
  "success": false,
  "error": "INVALID_TOKEN",
  "message": "Device token is invalid or revoked."
}
```

The ESP32 calls `/verify` on first boot or when a new token is entered via the portal. On success, the user info is cached locally and displayed on the portal dashboard (e.g. "Uploading as: Ryan (premium) — 158K lifetime readings on RadiaMaps"). This gives the user visual confirmation that their token is valid and linked to the right account.

### Authentication: Device Token

Simple token auth designed for embedded devices:

- User generates a **device token** from their RadiaMaps profile page
- Token displayed once (user copies to RadiaLog config via portal)
- Stored hashed in RadiaMaps `users` table (one token per user, or one per device)
- Sent via `X-Device-Token` header on every upload and verify call
- Server validates: hash lookup → user_id → authorized
- Regenerate to revoke. No expiry, no rate limit tiers, no Redis overhead.

### Required RadiaMaps Changes

1. **New route file: `routes/radialog.js`**
   - `POST /api/radialog/upload` — Accept batch readings (max 10K per request) with device token auth. Validate token → resolve user_id → call existing `dataModel.writeReadingsBatch()` (reuse data pipeline). Return accepted count.
   - `POST /api/radialog/verify` — Validate device token, return user profile info (username, nickname, member_since, total_readings, subscription_status). Used by ESP32 to confirm token and display user info on portal.

2. **Device token system**
   - Add `device_token_hash` column to `users` table (nullable, VARCHAR(64))
   - Profile page: "Generate Device Token" button → shows token once
   - `POST /api/radialog/token/generate` — Generate new token (requires OIDC auth)
   - `GET /api/radialog/token/status` — Check if token exists (requires OIDC auth)
   - `DELETE /api/radialog/token` — Revoke token (requires OIDC auth)

3. **Profile UI update**
   - Show "RadiaLog Device Token" section on profile
   - Generate / Regenerate / Revoke buttons
   - Copy-to-clipboard for the token

## Status Portal (WiFi AP)

Always available at `192.168.4.1` on the RadiaLog AP network. Serves as both monitoring dashboard and debug console.

### Portal Pages

**Dashboard (home)**
- **RadiaMaps account:** Username, nickname, subscription status, lifetime readings on RadiaMaps (fetched via `/api/radialog/verify` on token entry, cached locally)
- Current dose rate + count rate (live from RadiaCode USB)
- GPS fix status + current coordinates + satellite count
- WiFi STA connection status + signal strength
- Last successful upload timestamp
- Readings in buffer (pending upload)
- Lifetime readings logged (this device)
- Lifetime readings uploaded (this device)
- RadiaCode USB connection status
- Battery voltage (if available)

**Debug Console**
- Live scrolling log output (same as what would go to serial)
- Filter by module: `[USB]`, `[GPS]`, `[WIFI]`, `[UPLOAD]`, `[BUFFER]`
- Log level toggle: ERROR / WARN / INFO / DEBUG
- WebSocket-based for real-time streaming
- Last N log entries persisted in circular buffer (viewable even after reconnect)

**Settings**
- WiFi networks (add/remove, support 3 saved networks)
- RadiaMaps device token (paste + save)
- RadiaMaps API URL (default: `https://radiamaps.com/api/radialog/upload`)
- Device name / label
- Reading interval (default: 2000ms)
- GPS module type (v1: ATGM336H only, future: dropdown)

**Data View**
- Recent readings table (last 100)
- Mini chart: dose rate over time
- Export buffer as CSV (download)

**Actions**
- Force upload now
- Clear reading buffer
- Reboot device
- OTA firmware update (upload .bin file)
- Factory reset (confirm dialog)

## Configuration

Stored in LittleFS as `/config.json`. Editable via Status Portal.

```json
{
  "wifi": [
    {"ssid": "HomeNetwork", "password": "xxx"},
    {"ssid": "PhoneHotspot", "password": "xxx"}
  ],
  "radiamaps": {
    "token": "device_auth_token_here",
    "url": "https://radiamaps.com/api/radialog/upload",
    "device_id": "RadiaLog-A1B2"
  },
  "device": {
    "name": "RadiaLog-01",
    "reading_interval_ms": 2000,
    "gps_module": "atgm336h"
  },
  "ap": {
    "password": ""
  }
}
```

## Development Environment

- **Framework:** PlatformIO (Arduino framework)
- **Board:** XIAO ESP32S3 (seeed_xiao_esp32s3)
- **Build flags:** `-DARDUINO_USB_MODE=0` (enable USB OTG/Host via TinyUSB)
- **Debug:** FTDI USB-to-UART adapter on TX/RX pads during development. WiFi debug console for field debugging.
- **Libraries:**
  - `esp_usb_host` / ESP-IDF USB Host driver (ESP32-S3 native)
  - `TinyGPSPlus` — NMEA parsing for ATGM336H
  - `ArduinoJson` — JSON serialization for API payloads + config
  - `LittleFS` — Flash filesystem for reading buffer + config
  - `HTTPClient` — HTTPS uploads to RadiaMaps
  - `ESPAsyncWebServer` — Status portal web server
  - `AsyncWebSocket` — WebSocket for debug console streaming
  - `DNSServer` — Captive portal redirect
- **Custom code:**
  - RadiaCode USB protocol (C++ port of cdump/radiacode transport + data_buf decoder)
  - GPS driver abstraction layer

## Project Structure

```
RadiaLog/
├── PROJECT.md              ← This file
├── firmware/
│   ├── platformio.ini      ← PlatformIO config
│   └── src/
│       ├── main.cpp        ← Entry point, setup/loop
│       ├── config.h        ← Pin definitions, defaults
│       ├── radiacode.h/cpp ← RadiaCode USB Host protocol
│       ├── gps/
│       │   ├── gps.h       ← GPS interface (abstract)
│       │   ├── atgm336h.h/cpp ← ATGM336H UART driver (v1)
│       │   └── (future: ublox_i2c.h/cpp, etc.)
│       ├── buffer.h/cpp    ← Flash-backed reading storage + stats
│       ├── uploader.h/cpp  ← WiFi STA + HTTP batch upload
│       ├── portal/
│       │   ├── portal.h/cpp    ← Web server setup + routes
│       │   ├── debug_ws.h/cpp  ← WebSocket debug console
│       │   └── html/           ← Embedded HTML/CSS/JS
│       ├── wifi_mgr.h/cpp  ← AP+STA dual mode manager
│       ├── config_mgr.h/cpp← JSON config load/save (LittleFS)
│       └── led.h/cpp       ← Status LED patterns
├── hardware/
│   ├── BOM.md              ← Bill of materials
│   └── wiring.md           ← Pin connections (UART GPS, USB, etc.)
├── docs/
│   ├── setup.md            ← Getting started guide
│   └── reference/
│       ├── radiacode_protocol.md  ← Full USB protocol docs
│       └── usb_transport.py       ← Python reference implementation
└── web/                    ← Source HTML/CSS/JS for portal (compiled into firmware)
```

## Phases

### Phase 1: USB Protocol & Proof of Concept
- [ ] Port RadiaCode USB protocol to C++ (transport layer + init sequence)
- [ ] ESP32-S3 USB Host connects to RadiaCode, reads DATA_BUF
- [ ] Decode RealTimeData records (dose_rate, count_rate)
- [ ] ATGM336H GPS connected via UART, reads lat/lon
- [ ] Readings printed to UART serial console (via FTDI)
- [ ] Verify USB Host works on XIAO ESP32S3 with OTG adapter

### Phase 2: Buffer & Upload
- [ ] LittleFS reading buffer (persist across power cycles)
- [ ] AP+STA dual WiFi mode
- [ ] Batch upload buffered readings on STA connect
- [ ] Mark uploaded readings, don't re-upload
- [ ] Track stats: buffer depth, lifetime logged, lifetime uploaded
- [ ] Status LED feedback
- [ ] **RadiaMaps: new `/api/radialog/upload` route + device token system**

### Phase 3: Status Portal
- [ ] Web server on AP interface (ESPAsyncWebServer)
- [ ] Dashboard page (live stats, buffer status, upload status)
- [ ] Debug console page (WebSocket log streaming)
- [ ] Settings page (WiFi, auth token, device config)
- [ ] Data view page (recent readings, mini chart, CSV export)
- [ ] Action buttons (force upload, clear buffer, reboot, OTA)

### Phase 4: Polish & Production
- [ ] Multiple GPS module support (driver abstraction tested with 2+ modules)
- [ ] Power optimization (deep sleep between readings when stationary)
- [ ] Battery voltage monitoring + display on portal
- [ ] Enclosure design (3D printed, compact)
- [ ] LoRa integration via Wio-SX1262 (mesh radiation data sharing, future)
- [ ] RadiaMaps: device management page (see all registered devices, last seen, upload history)

## Open Questions

1. **VBUS power** — Does RadiaCode need 5V VBUS from the ESP32 to enumerate? If yes, need external 5V supply on the USB line (XIAO's 3.3V won't cut it).
2. **Device token: per-user or per-device?** Current design is one token per user. Could extend to multiple tokens (one per physical RadiaLog device) if needed.
3. **AP portal security** — Open AP or password-protected? Default open for ease of use, with optional AP password in config.
4. **Enclosure** — 3D printed case to hold XIAO + Wio-SX1262 + GPS + battery, mountable on/near RadiaCode?
5. **Buffer full policy** — When buffer hits capacity and device hasn't uploaded: stop logging? Overwrite oldest uploaded readings? Overwrite oldest regardless? (Current design: prune oldest uploaded readings first, never prune unuploaded.)

## Dependencies / Prerequisites

- [x] XIAO ESP32S3 + Wio-SX1262 kit (Ryan has this)
- [ ] ATGM336H GPS module
- [ ] USB-C OTG adapter/cable (ESP32 → RadiaCode)
- [ ] FTDI USB-to-UART adapter (development only)
- [ ] RadiaCode device for testing
- [ ] LiPo battery
- [ ] **RadiaMaps: `/api/radialog/upload` route + device token system**

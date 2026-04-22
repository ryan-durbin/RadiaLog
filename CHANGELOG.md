# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- AP portal authentication with default password `"radialog"` (configurable via Settings page)
- Battery charging state indicator on dashboard and self-test pages (AXP2101 boards)
- `/api/status` now includes `battery_charging` field

### Changed
- Removed LoRa pin definitions from `config.h` (Wio-SX1262 pins no longer reserved)

### Security
- AP portal is now password-protected by default instead of open

## [0.2.4] - 2025-03-24

### Added
- Self-test page (`/self-test`) with guided hardware QA checks (battery, GPS fix, WiFi, BLE scan, buttons)
- Factory reset endpoint (`POST /api/actions/factory-reset`) that wipes config and buffer
- Portal IP changed to `10.0.0.1` for consistency across all boards
- GPS power control: software standby command (`$PMTK161,0`) for ATGM336H shutdown
- README documentation refresh with troubleshooting section

### Changed
- GPS power can now be cut via hardware ON/OFF pin (GPIO5) on XIAO boards in addition to software standby
- Updated purchase links and assembly instructions in docs

## [0.2.3] - 2025-03-18

### Added
- WiFi geolocation fallback: when GPS is unavailable, uses Google Maps Geolocation API with STA RSSI data to estimate position
- `google_api_key` setting for WiFi geolocation fallback (Settings page)

## [0.2.2] - 2025-03-10

### Added
- Heltec Wireless Tracker V1.1 board support (`BOARD_HELTEC_TRACKER`) with UC6580 GNSS driver
- Display wake on USB charging detection (boards with `HAS_DISPLAY`)
- Lifetime readings logged counter ("gamification" display on dashboard)

### Fixed
- Buffer mutex safety: all LittleFS file I/O now protected by FreeRTOS mutex

## [0.2.1] - 2025-03-04

### Added
- Shipping mode: long-press boot button (5s) enters deep sleep with LED confirmation blink pattern
- BLE device list management via Settings page (up to 4 MAC addresses)
- `button_wake` setting to re-enable display on button press after timeout

### Fixed
- Buffer pruning now correctly preserves unuploaded readings when approaching capacity

## [0.2.0] - 2025-02-20

### Added
- Multi-device support: USB + up to 4 BLE RadiaCode connections simultaneously via `RadiaCodeMgr`
- NTP time sync with GPS fallback for initial timestamp
- Upload manager FreeRTOS task on core 0 with exponential backoff retry logic
- WebSocket debug console (`/debug`) with live log streaming and module filtering
- Data export page (`/data`) with CSV download

### Changed
- Buffer storage format: compact 34-byte binary records across three files for power-fail resilience
- CPU frequency reduced to 80MHz; tickless idle enabled via ESP-IDF PM framework
- AP auto-disables after 5 minutes of no connected clients (configurable)

## [0.1.0] - 2025-01-15

### Added
- Initial release: single RadiaCode USB connection, GPS datalogging, LittleFS buffer storage
- Web portal with dashboard, settings, and OTA firmware update
- Support for Seeed XIAO ESP32S3, XIAO ESP32S3 Plus, LilyGO T-Display S3, Waveshare Round

[Unreleased]: https://github.com/radianlog/radialog/compare/v0.2.4...HEAD
[0.2.4]: https://github.com/radianlog/radialog/compare/v0.2.3...v0.2.4
[0.2.3]: https://github.com/radianlog/radialog/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/radianlog/radialog/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/radianlog/radialog/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/radianlog/radialog/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/radianlog/radialog/releases/tag/v0.1.0

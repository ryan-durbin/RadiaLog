#pragma once
#include "config.h"

#ifdef HAS_DISPLAY

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "display_gfx.h"

// =============================================================================
// RadiaLog Firmware - TFT Display Module
// Supports multiple display sizes via DISPLAY_WIDTH / DISPLAY_HEIGHT in config.h.
// Press button → show stats for N seconds → auto-off to save power.
// =============================================================================

/// Snapshot of system state passed to the display for rendering.
struct DisplayStatus {
    bool     rcConnected;
    bool     rcIsUsb;           ///< true = USB source, false = BLE source
    bool     wifiConnected;
    String   wifiSSID;
    String   staIP;
    String   lastUpload;
    uint32_t totalReadings;
    uint32_t pendingReadings;
    uint64_t totalReadingsLogged;  ///< Lifetime readings counter (survives reboots)
    float    doseRate;
    float    countRate;
    float    batteryVoltage;
    uint8_t  batteryPercent;
    bool     gpsFix;
    int      gpsSats;
    String   timeSyncSource;    ///< "" = not synced, "NTP", "GPS"
    uint32_t loopCount;         ///< Main loop iteration counter (debug)
};

class Display {
public:
    Display();

    /// Initialize TFT, backlight, and button pin.
    void begin();

    /// Check button press and manage display timeout. Call every loop().
    void handleButton();

    /// Redraw the status screen (only if display is on).
    void draw(const DisplayStatus& status);

    /// True if the display is currently powered on.
    bool isOn() const { return _on; }

    /// Set display timeout in seconds. 0 = always on (default).
    void setTimeoutSec(uint16_t sec);

    /// Enable/disable physical button for display wake (default: true).
    void setButtonWakeEnabled(bool enabled);

    /// Wake the display (e.g. when USB power is connected). No-op if already on or no timeout set.
    void wake();

private:
    GfxDriver   _tft;
    GfxSprite   _sprite;    // Persistent off-screen buffer (allocated once in begin)
    bool     _on;
    unsigned long _wakeTime;
    bool     _lastBtnState;
    unsigned long _lastDebounce;
    bool     _lastBtn2State;
    unsigned long _lastDebounce2;

    uint16_t _timeoutSec;           // 0 = always on
    bool     _buttonWakeEnabled;

    static constexpr unsigned long DEBOUNCE_MS = 50;

    void _wake();
    void _sleep();
    void _drawRectangular(const DisplayStatus& s);
    void _drawCircular(const DisplayStatus& s);

#ifdef HAS_TOUCH
    static void IRAM_ATTR _touchISR();
    static volatile bool _touchFlag;
#endif
};

#endif // DISPLAY_H
#endif // HAS_DISPLAY

#pragma once
#include "config.h"

#ifdef HAS_DISPLAY

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// =============================================================================
// RadiaLog Firmware - TFT Display Module
// LilyGO T-Display S3: 170x320 ST7789, button-activated status screen.
// Press button → show stats for 10s → auto-off to save power.
// =============================================================================

/// Snapshot of system state passed to the display for rendering.
struct DisplayStatus {
    bool     usbConnected;
    bool     wifiConnected;
    String   wifiSSID;
    String   staIP;
    String   lastUpload;
    uint32_t totalReadings;
    uint32_t pendingReadings;
    float    doseRate;
    float    countRate;
    float    batteryVoltage;
    uint8_t  batteryPercent;
    bool     gpsFix;
    int      gpsSats;
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

private:
    TFT_eSPI _tft;
    bool     _on;
    unsigned long _wakeTime;
    bool     _lastBtnState;
    unsigned long _lastDebounce;

    static constexpr unsigned long DISPLAY_TIMEOUT_MS = 10000;
    static constexpr unsigned long DEBOUNCE_MS = 50;

    void _wake();
    void _sleep();

#ifdef HAS_TOUCH
    static void IRAM_ATTR _touchISR();
    static volatile bool _touchFlag;
#endif
};

#endif // DISPLAY_H
#endif // HAS_DISPLAY

#include "config.h"

#ifdef HAS_DISPLAY

#include "display.h"

// =============================================================================
// RadiaLog Firmware - TFT Display Implementation
// Draws a dark-themed status screen. Layout adapts to DISPLAY_WIDTH/HEIGHT.
// =============================================================================

// Shorthand for layout math
static constexpr int DW = DISPLAY_WIDTH;
static constexpr int DH = DISPLAY_HEIGHT;

// Color palette (RGB565)
static const uint16_t COL_BG      = 0x0000;  // Black
static const uint16_t COL_TITLE   = 0xFFFF;  // White
static const uint16_t COL_LABEL   = 0x7BEF;  // Light gray
static const uint16_t COL_DIM     = 0x4208;  // Dark gray
static const uint16_t COL_VALUE   = 0xFFFF;  // White
static const uint16_t COL_GREEN   = 0x07E0;  // Green
static const uint16_t COL_RED     = 0xF800;  // Red
static const uint16_t COL_YELLOW  = 0xFDA0;  // Yellow
static const uint16_t COL_LINE    = 0x2104;  // Separator line

#ifdef HAS_TOUCH
volatile bool Display::_touchFlag = false;

void IRAM_ATTR Display::_touchISR() {
    _touchFlag = true;
}
#endif

Display::Display()
    : _tft()
    , _sprite(&_tft)
    , _on(false)
    , _wakeTime(0)
    , _lastBtnState(HIGH)
    , _lastDebounce(0)
    , _lastBtn2State(HIGH)
    , _lastDebounce2(0)
    , _timeoutSec(0)
    , _buttonWakeEnabled(true)
{
}

void Display::setTimeoutSec(uint16_t sec) {
    _timeoutSec = sec;
    // If switching to always-on and currently off, wake up
    if (sec == 0 && !_on) {
        _wake();
    }
}

void Display::setButtonWakeEnabled(bool enabled) {
    _buttonWakeEnabled = enabled;
}

void Display::wake() {
    if (!_on && _timeoutSec > 0) {
        _wake();
    }
}

void Display::begin() {
    // Enable display power (some boards have a separate power gate)
#if TFT_POWER_PIN >= 0
    pinMode(TFT_POWER_PIN, OUTPUT);
    digitalWrite(TFT_POWER_PIN, HIGH);
#endif

#if defined(DISPLAY_DRIVER_LGFX) || defined(DISPLAY_DRIVER_ARDUINO_GFX)
    // LovyanGFX / Arduino_GFX manage backlight via display command — do NOT set pinMode/digitalWrite
#else
    // Backlight pin (TFT_eSPI: direct GPIO or PWM)
    pinMode(TFT_BL_PIN, OUTPUT);
#endif

    // Buttons with internal pullup
#ifdef HAS_BUTTON
    pinMode(BUTTON_PIN, INPUT_PULLUP);
#if BUTTON2_PIN >= 0
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
#endif
#endif

    _tft.init();
    _tft.setRotation(0);  // Portrait
    _tft.fillScreen(COL_BG);

    // Initialize touch controller
#ifdef HAS_TOUCH
    // Reset the CST816S
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(10);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(50);

    // Touch interrupt — fires on any touch (falling edge)
    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_INT_PIN), _touchISR, FALLING);
#endif

    // Allocate off-screen sprite once (DW * DH * 2 bytes)
    _sprite.createSprite(DW, DH);

    // Show boot message and start with display on
    _tft.setTextColor(COL_TITLE, COL_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(2);
    _tft.drawString("RadiaLog", DW / 2, DH * 7 / 16);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_DIM, COL_BG);
    _tft.drawString("v" FW_VERSION, DW / 2, DH * 7 / 16 + 25);
    _tft.drawString("Starting...", DW / 2, DH * 7 / 16 + 45);
    _tft.setTextDatum(TL_DATUM);

    _wake();
}

void Display::handleButton() {
    unsigned long now = millis();
    bool wakeTriggered = false;

    // Check physical buttons — only if button wake is enabled
#ifdef HAS_BUTTON
    if (_buttonWakeEnabled) {
        // Button 1
        bool reading = digitalRead(BUTTON_PIN);
        if (reading != _lastBtnState) {
            _lastDebounce = now;
        }
        if ((now - _lastDebounce) > DEBOUNCE_MS) {
            if (reading == LOW && _lastBtnState == HIGH) {
                wakeTriggered = true;
            }
        }
        _lastBtnState = reading;

#if BUTTON2_PIN >= 0
        // Button 2
        bool reading2 = digitalRead(BUTTON2_PIN);
        if (reading2 != _lastBtn2State) {
            _lastDebounce2 = now;
        }
        if ((now - _lastDebounce2) > DEBOUNCE_MS) {
            if (reading2 == LOW && _lastBtn2State == HIGH) {
                wakeTriggered = true;
            }
        }
        _lastBtn2State = reading2;
#endif
    }
#endif

    // Check touch interrupt
#ifdef HAS_TOUCH
    if (_touchFlag) {
        _touchFlag = false;
        wakeTriggered = true;
    }
#endif

    // Toggle display on wake trigger (only when timeout is active)
    if (wakeTriggered && _timeoutSec > 0) {
        if (_on) {
            _sleep();
        } else {
            _wake();
        }
    }

    // Auto-off timeout (0 = always on, skip)
    if (_timeoutSec > 0 && _on) {
        unsigned long timeoutMs = static_cast<unsigned long>(_timeoutSec) * 1000UL;
        if ((now - _wakeTime) > timeoutMs) {
            _sleep();
        }
    }
}

void Display::_wake() {
    _on = true;
    _wakeTime = millis();
#if defined(DISPLAY_DRIVER_LGFX) || defined(DISPLAY_DRIVER_ARDUINO_GFX)
    // LovyanGFX / Arduino_GFX: use setBrightness for display-command backlight control
    _tft.setBrightness(200);
#else
    digitalWrite(TFT_BL_PIN, HIGH);
#endif
}

void Display::_sleep() {
    _on = false;
#if defined(DISPLAY_DRIVER_LGFX) || defined(DISPLAY_DRIVER_ARDUINO_GFX)
    _tft.setBrightness(0);
#else
    digitalWrite(TFT_BL_PIN, LOW);
#endif
}

// =============================================================================
// Helper: pick dose rate color by severity
// =============================================================================
static uint16_t doseColor(float doseRate_uSvh) {
    if (doseRate_uSvh >= 1.0f)  return 0xF800;  // Red:    >= 1 uSv/h
    if (doseRate_uSvh >= 0.3f)  return 0xFDA0;  // Yellow: >= 0.3
    return 0x07E0;                                // Green:  normal
}

// =============================================================================
// draw() — render the status screen
// Dispatches to rectangular or circular layout based on DISPLAY_CIRCULAR.
// =============================================================================

void Display::draw(const DisplayStatus& s) {
    if (!_on) return;

#ifdef DISPLAY_CIRCULAR
    _drawCircular(s);
#else
    _drawRectangular(s);
#endif
}

// =============================================================================
// Rectangular layout (portrait screens: T-Display S3, AMOLED, etc.)
//
//   [status bar]         — compact row of icons + battery
//   [dose rate HERO]     — big number, color-coded
//   [count rate]         — secondary reading
//   [info rows]          — readings, pending, upload
// =============================================================================

void Display::_drawRectangular(const DisplayStatus& s) {
    // Layout constants derived from display size
    const int margin  = DW > 200 ? 12 : 8;         // side margin
    const int rightX  = DW - margin - 2;            // right-align anchor
    const int lineW   = DW - 2 * margin;            // separator width

    // Draw into the persistent off-screen sprite, then push once — no flicker
    GfxSprite& fb = _sprite;
    fb.fillSprite(COL_BG);
    fb.setTextDatum(TL_DATUM);

    // =====================================================================
    // STATUS BAR (y: 0–28)
    // =====================================================================
    int sx = margin;
    int sy = 8;

    // RadiaCode indicator — green=connected, red=disconnected
    fb.fillCircle(sx + 4, sy + 4, 4, s.rcConnected ? COL_GREEN : COL_RED);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(1);
    fb.drawString(s.rcConnected ? (s.rcIsUsb ? "USB" : "BLE") : "RC", sx + 12, sy);

    // WiFi indicator
    sx += 38;
    fb.fillCircle(sx + 4, sy + 4, 4, s.wifiConnected ? COL_GREEN : COL_RED);
    fb.drawString("Wi", sx + 12, sy);

    // GPS indicator
    sx += 34;
    fb.fillCircle(sx + 4, sy + 4, 4, s.gpsFix ? COL_GREEN : COL_YELLOW);
    if (s.gpsFix) {
        fb.drawString(String(s.gpsSats), sx + 12, sy);
    } else {
        fb.drawString("--", sx + 12, sy);
    }

    // Battery (right side)
    if (s.batteryVoltage > 2.0f) {
        uint16_t batCol = s.batteryPercent > 20 ? COL_GREEN
                        : s.batteryPercent > 5  ? COL_YELLOW : COL_RED;
        char batBuf[8];
        snprintf(batBuf, sizeof(batBuf), "%d%%", s.batteryPercent);
        fb.setTextColor(batCol, COL_BG);
        int bw = fb.textWidth(batBuf);
        fb.drawString(batBuf, rightX - bw, sy);

        // Tiny battery bar
        int barW = DW > 200 ? 44 : 34;
        int barX = rightX - barW, barY = sy + 10, barH = 5;
        fb.drawRect(barX, barY, barW, barH, COL_DIM);
        int fillW = (s.batteryPercent * (barW - 2)) / 100;
        if (fillW > 0) {
            fb.fillRect(barX + 1, barY + 1, fillW, barH - 2, batCol);
        }
    }

    fb.drawFastHLine(margin, 26, lineW, COL_LINE);

    // =====================================================================
    // DOSE RATE — hero section
    // =====================================================================
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(2);
    int labelW = fb.textWidth("DOSE RATE");
    fb.drawString("DOSE RATE", (DW - labelW) / 2, 34);

    // Big number — color-coded, size 4, displayed in nSv/h
    uint16_t dCol = doseColor(s.doseRate);
    float doseNsvh = s.doseRate * 1000.0f;
    char doseBuf[16];
    if (doseNsvh < 100.0f) {
        snprintf(doseBuf, sizeof(doseBuf), "%.1f", doseNsvh);
    } else if (doseNsvh < 10000.0f) {
        snprintf(doseBuf, sizeof(doseBuf), "%.0f", doseNsvh);
    } else {
        snprintf(doseBuf, sizeof(doseBuf), "%.0f", doseNsvh);
    }
    fb.setTextColor(dCol, COL_BG);
    fb.setTextSize(4);
    int doseW = fb.textWidth(doseBuf);
    fb.drawString(doseBuf, (DW - doseW) / 2, 58);

    // Unit
    fb.setTextColor(COL_DIM, COL_BG);
    fb.setTextSize(2);
    int unitW = fb.textWidth("nSv/h");
    fb.drawString("nSv/h", (DW - unitW) / 2, 98);

    // =====================================================================
    // COUNT RATE
    // =====================================================================
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(2);
    fb.drawString("CPS", margin, 128);

    char cpsBuf[16];
    snprintf(cpsBuf, sizeof(cpsBuf), "%.2f", s.countRate);
    fb.setTextColor(COL_VALUE, COL_BG);
    int cpsW = fb.textWidth(cpsBuf);
    fb.drawString(cpsBuf, rightX - cpsW, 128);

    fb.drawFastHLine(margin, 150, lineW, COL_LINE);

    // =====================================================================
    // INFO ROWS — size 2 for readability
    // =====================================================================
    fb.setTextSize(2);
    int iy = 158;
    const int rowH = 24;

    // Readings stored
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Stored", margin, iy);
    fb.setTextColor(COL_VALUE, COL_BG);
    String storedStr = String(s.totalReadings);
    fb.drawString(storedStr, rightX - fb.textWidth(storedStr), iy);
    iy += rowH;

    // Pending upload
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Pending", margin, iy);
    fb.setTextColor(s.pendingReadings > 0 ? COL_YELLOW : COL_GREEN, COL_BG);
    String pendStr = String(s.pendingReadings);
    fb.drawString(pendStr, rightX - fb.textWidth(pendStr), iy);
    iy += rowH;

    // Last upload (or "No Token" warning if uploads disabled)
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Upload", margin, iy);
    if (!s.uploadEnabled) {
        fb.setTextColor(COL_RED, COL_BG);
        String noTok = "No Token";
        fb.drawString(noTok, rightX - fb.textWidth(noTok), iy);
    } else {
        fb.setTextColor(COL_VALUE, COL_BG);
        fb.drawString(s.lastUpload, rightX - fb.textWidth(s.lastUpload), iy);
    }
    iy += rowH;

    // Lifetime readings logged
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Logged", margin, iy);
    fb.setTextColor(COL_GREEN, COL_BG);
    char logBuf[16];
    if (s.totalReadingsLogged >= 1000000ULL) {
        snprintf(logBuf, sizeof(logBuf), "%.1fM", s.totalReadingsLogged / 1000000.0);
    } else if (s.totalReadingsLogged >= 1000ULL) {
        snprintf(logBuf, sizeof(logBuf), "%.1fK", s.totalReadingsLogged / 1000.0);
    } else {
        snprintf(logBuf, sizeof(logBuf), "%llu", (unsigned long long)s.totalReadingsLogged);
    }
    String logStr(logBuf);
    fb.drawString(logStr, rightX - fb.textWidth(logStr), iy);
    iy += rowH;

    // Time sync source
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Sync", margin, iy);
    if (s.timeSyncSource.length() > 0) {
        fb.setTextColor(COL_GREEN, COL_BG);
        fb.drawString(s.timeSyncSource, rightX - fb.textWidth(s.timeSyncSource), iy);
    } else {
        fb.setTextColor(COL_YELLOW, COL_BG);
        String noSync = "---";
        fb.drawString(noSync, rightX - fb.textWidth(noSync), iy);
    }
    iy += rowH;

    fb.drawFastHLine(margin, iy + 2, lineW, COL_LINE);

    // =====================================================================
    // WIFI + UPTIME (bottom, small)
    // =====================================================================
    iy += 6;
    fb.setTextSize(1);
    fb.setTextColor(COL_DIM, COL_BG);
    if (s.wifiConnected && s.staIP.length() > 0) {
        int ssidW = fb.textWidth(s.wifiSSID);
        fb.drawString(s.wifiSSID, (DW - ssidW) / 2, iy);
        int ipW = fb.textWidth(s.staIP);
        fb.drawString(s.staIP, (DW - ipW) / 2, iy + 12);
    } else {
        int nwW = fb.textWidth("WiFi disconnected");
        fb.drawString("WiFi disconnected", (DW - nwW) / 2, iy);
    }

    // Uptime — bottom-right corner
    unsigned long sec = millis() / 1000UL;
    char uptBuf[16];
    if (sec < 3600) {
        snprintf(uptBuf, sizeof(uptBuf), "%lum%lus", sec / 60, sec % 60);
    } else if (sec < 86400) {
        snprintf(uptBuf, sizeof(uptBuf), "%luh%lum", sec / 3600, (sec % 3600) / 60);
    } else {
        snprintf(uptBuf, sizeof(uptBuf), "%lud%luh", sec / 86400, (sec % 86400) / 3600);
    }
    int upW = fb.textWidth(uptBuf);
    fb.drawString(uptBuf, rightX - upW, DH - 10);

    // Push the whole frame at once
    fb.pushSprite(0, 0);
}

// =============================================================================
// Circular layout (round displays: GC9A01 240x240, etc.)
//
// Compact centered design that fits within the inscribed circle:
//   [status dots]        — RC / WiFi / GPS indicators across the top
//   [dose rate HERO]     — big centered number
//   [CPS + battery]      — secondary row
//   [info rows]          — stored / pending / sync (compact)
//   [uptime]             — bottom center
// =============================================================================

void Display::_drawCircular(const DisplayStatus& s) {
    const int cx = DW / 2;   // center X
    const int cy = DH / 2;   // center Y
    const int r  = DW / 2;   // radius

    GfxSprite& fb = _sprite;
    fb.fillSprite(COL_BG);

    // Draw a subtle circle border to frame the display
    fb.drawCircle(cx, cy, r - 1, COL_LINE);

    // =====================================================================
    // STATUS INDICATORS — row of colored dots near the top
    // Layout: 3 dots evenly spaced across the safe area
    // =====================================================================
    fb.setTextDatum(TC_DATUM);
    int dotY = 28;
    int dotSpacing = 50;
    int dotStartX = cx - dotSpacing;

    // RadiaCode
    fb.fillCircle(dotStartX, dotY, 5, s.rcConnected ? COL_GREEN : COL_RED);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(1);
    fb.drawString(s.rcConnected ? (s.rcIsUsb ? "USB" : "BLE") : "RC", dotStartX, dotY + 9);

    // WiFi
    fb.fillCircle(cx, dotY, 5, s.wifiConnected ? COL_GREEN : COL_RED);
    fb.drawString("WiFi", cx, dotY + 9);

    // GPS
    fb.fillCircle(dotStartX + 2 * dotSpacing, dotY, 5, s.gpsFix ? COL_GREEN : COL_YELLOW);
    fb.drawString(s.gpsFix ? String(s.gpsSats) : "--", dotStartX + 2 * dotSpacing, dotY + 9);

    // =====================================================================
    // DOSE RATE — hero section (centered in the circle)
    // =====================================================================
    fb.setTextDatum(TC_DATUM);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(1);
    fb.drawString("DOSE RATE", cx, 54);

    // Big number — color-coded
    uint16_t dCol = doseColor(s.doseRate);
    float doseNsvh = s.doseRate * 1000.0f;
    char doseBuf[16];
    if (doseNsvh < 100.0f) {
        snprintf(doseBuf, sizeof(doseBuf), "%.1f", doseNsvh);
    } else {
        snprintf(doseBuf, sizeof(doseBuf), "%.0f", doseNsvh);
    }
    fb.setTextColor(dCol, COL_BG);
    fb.setTextSize(4);
    fb.drawString(doseBuf, cx, 68);

    // Unit
    fb.setTextColor(COL_DIM, COL_BG);
    fb.setTextSize(1);
    fb.drawString("nSv/h", cx, 104);

    // =====================================================================
    // CPS + BATTERY — secondary row
    // =====================================================================
    fb.setTextSize(2);
    int midY = 118;

    // CPS (left of center)
    char cpsBuf[16];
    snprintf(cpsBuf, sizeof(cpsBuf), "%.1f", s.countRate);
    fb.setTextDatum(TR_DATUM);
    fb.setTextColor(COL_VALUE, COL_BG);
    fb.drawString(cpsBuf, cx - 4, midY);
    fb.setTextSize(1);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("CPS", cx - 4, midY + 18);

    // Battery (right of center)
    if (s.batteryVoltage > 2.0f) {
        uint16_t batCol = s.batteryPercent > 20 ? COL_GREEN
                        : s.batteryPercent > 5  ? COL_YELLOW : COL_RED;
        char batBuf[8];
        snprintf(batBuf, sizeof(batBuf), "%d%%", s.batteryPercent);
        fb.setTextDatum(TL_DATUM);
        fb.setTextColor(batCol, COL_BG);
        fb.setTextSize(2);
        fb.drawString(batBuf, cx + 4, midY);
        fb.setTextSize(1);
        fb.setTextColor(COL_LABEL, COL_BG);
        fb.drawString("BAT", cx + 4, midY + 18);
    }

    // =====================================================================
    // INFO ROWS — compact, centered within circle safe area
    // =====================================================================
    fb.setTextDatum(TL_DATUM);
    fb.setTextSize(1);
    int iy = 148;
    const int rowH = 14;
    const int leftX  = cx - 70;   // safe area within circle
    const int rgtX   = cx + 70;

    // Stored
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Stored", leftX, iy);
    fb.setTextColor(COL_VALUE, COL_BG);
    String storedStr = String(s.totalReadings);
    fb.drawString(storedStr, rgtX - fb.textWidth(storedStr), iy);
    iy += rowH;

    // Pending (or "No Token" if uploads disabled)
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Pending", leftX, iy);
    if (!s.uploadEnabled) {
        fb.setTextColor(COL_RED, COL_BG);
        String noTok = "No Token";
        fb.drawString(noTok, rgtX - fb.textWidth(noTok), iy);
    } else {
        fb.setTextColor(s.pendingReadings > 0 ? COL_YELLOW : COL_GREEN, COL_BG);
        String pendStr = String(s.pendingReadings);
        fb.drawString(pendStr, rgtX - fb.textWidth(pendStr), iy);
    }
    iy += rowH;

    // Logged
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Logged", leftX, iy);
    fb.setTextColor(COL_GREEN, COL_BG);
    char logBuf[16];
    if (s.totalReadingsLogged >= 1000000ULL) {
        snprintf(logBuf, sizeof(logBuf), "%.1fM", s.totalReadingsLogged / 1000000.0);
    } else if (s.totalReadingsLogged >= 1000ULL) {
        snprintf(logBuf, sizeof(logBuf), "%.1fK", s.totalReadingsLogged / 1000.0);
    } else {
        snprintf(logBuf, sizeof(logBuf), "%llu", (unsigned long long)s.totalReadingsLogged);
    }
    String logStr(logBuf);
    fb.drawString(logStr, rgtX - fb.textWidth(logStr), iy);
    iy += rowH;

    // Sync
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Sync", leftX, iy);
    if (s.timeSyncSource.length() > 0) {
        fb.setTextColor(COL_GREEN, COL_BG);
        fb.drawString(s.timeSyncSource, rgtX - fb.textWidth(s.timeSyncSource), iy);
    } else {
        fb.setTextColor(COL_YELLOW, COL_BG);
        String noSync = "---";
        fb.drawString(noSync, rgtX - fb.textWidth(noSync), iy);
    }

    // =====================================================================
    // UPTIME — bottom center
    // =====================================================================
    fb.setTextDatum(BC_DATUM);
    fb.setTextColor(COL_DIM, COL_BG);
    fb.setTextSize(1);
    unsigned long sec = millis() / 1000UL;
    char uptBuf[16];
    if (sec < 3600) {
        snprintf(uptBuf, sizeof(uptBuf), "%lum%lus", sec / 60, sec % 60);
    } else if (sec < 86400) {
        snprintf(uptBuf, sizeof(uptBuf), "%luh%lum", sec / 3600, (sec % 3600) / 60);
    } else {
        snprintf(uptBuf, sizeof(uptBuf), "%lud%luh", sec / 86400, (sec % 86400) / 3600);
    }
    fb.drawString(uptBuf, cx, DH - 18);

    // Push the whole frame at once
    fb.pushSprite(0, 0);
}

#endif // HAS_DISPLAY

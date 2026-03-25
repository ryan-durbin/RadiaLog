#include "config.h"

#ifdef HAS_DISPLAY

#include "display.h"

// =============================================================================
// RadiaLog Firmware - TFT Display Implementation
// Draws a dark-themed status screen on the T-Display S3 (170x320 ST7789).
// =============================================================================

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
    : _tft(TFT_eSPI())
    , _sprite(&_tft)
    , _on(false)
    , _wakeTime(0)
    , _lastBtnState(HIGH)
    , _lastDebounce(0)
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

void Display::begin() {
    // Enable display power (required for battery operation)
    pinMode(TFT_POWER_PIN, OUTPUT);
    digitalWrite(TFT_POWER_PIN, HIGH);

    // Backlight pin
    pinMode(TFT_BL_PIN, OUTPUT);

    // Button with internal pullup
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    _tft.init();
    _tft.setRotation(0);  // Portrait: 170 wide x 320 tall
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

    // Allocate off-screen sprite once (170x320x2 = ~109KB)
    _sprite.createSprite(170, 320);

    // Show boot message and start with display on
    _tft.setTextColor(COL_TITLE, COL_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(2);
    _tft.drawString("RadiaLog", 85, 140);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_DIM, COL_BG);
    _tft.drawString("v" FW_VERSION, 85, 165);
    _tft.drawString("Starting...", 85, 185);
    _tft.setTextDatum(TL_DATUM);

    _wake();
}

void Display::handleButton() {
    unsigned long now = millis();
    bool wakeTriggered = false;

    // Check physical button (GPIO14) — only if button wake is enabled
    if (_buttonWakeEnabled) {
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
    }

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
    digitalWrite(TFT_BL_PIN, HIGH);
}

void Display::_sleep() {
    _on = false;
    digitalWrite(TFT_BL_PIN, LOW);
}

// =============================================================================
// Helper: pick dose rate color by severity
// =============================================================================
static uint16_t doseColor(float doseRate) {
    if (doseRate >= 1.0f)  return 0xF800;  // Red:    >= 1 uSv/h
    if (doseRate >= 0.3f)  return 0xFDA0;  // Yellow: >= 0.3
    return 0x07E0;                          // Green:  normal
}

// =============================================================================
// draw() — render the status screen
// Layout: 170 wide x 320 tall (portrait)
//
//   [status bar]         — compact row of icons + battery
//   [dose rate HERO]     — big number, color-coded
//   [count rate]         — secondary reading
//   [info rows]          — readings, pending, upload
// =============================================================================

void Display::draw(const DisplayStatus& s) {
    if (!_on) return;

    // Draw into the persistent off-screen sprite, then push once — no flicker
    TFT_eSprite& fb = _sprite;
    fb.fillSprite(COL_BG);
    fb.setTextDatum(TL_DATUM);

    // =====================================================================
    // STATUS BAR (y: 0–28)
    // =====================================================================
    int sx = 8;
    int sy = 8;

    // RadiaCode indicator
    fb.fillCircle(sx + 4, sy + 4, 4, s.usbConnected ? COL_GREEN : COL_RED);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(1);
    fb.drawString("RC", sx + 12, sy);

    // WiFi indicator
    sx += 38;
    fb.fillCircle(sx + 4, sy + 4, 4, s.wifiConnected ? COL_GREEN : COL_RED);
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Wi", sx + 12, sy);

    // GPS indicator
    sx += 34;
    fb.fillCircle(sx + 4, sy + 4, 4, s.gpsFix ? COL_GREEN : COL_YELLOW);
    fb.setTextColor(COL_LABEL, COL_BG);
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
        fb.setTextDatum(TR_DATUM);
        fb.drawString(batBuf, 164, sy);
        fb.setTextDatum(TL_DATUM);

        // Tiny battery bar
        int barX = 130, barY = sy + 10, barW = 34, barH = 5;
        fb.drawRect(barX, barY, barW, barH, COL_DIM);
        int fillW = (s.batteryPercent * (barW - 2)) / 100;
        if (fillW > 0) {
            fb.fillRect(barX + 1, barY + 1, fillW, barH - 2, batCol);
        }
    }

    fb.drawFastHLine(8, 26, 154, COL_LINE);

    // =====================================================================
    // DOSE RATE — hero section (y: 32–130)
    // =====================================================================

    // Label — centered manually
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(2);
    int labelW = fb.textWidth("DOSE RATE");
    fb.drawString("DOSE RATE", (170 - labelW) / 2, 34);

    // Big number — color-coded, size 4 (24x32 per char = fits 7 chars in 170px)
    uint16_t dCol = doseColor(s.doseRate);
    char doseBuf[16];
    if (s.doseRate < 10.0f) {
        snprintf(doseBuf, sizeof(doseBuf), "%.3f", s.doseRate);
    } else if (s.doseRate < 100.0f) {
        snprintf(doseBuf, sizeof(doseBuf), "%.2f", s.doseRate);
    } else {
        snprintf(doseBuf, sizeof(doseBuf), "%.1f", s.doseRate);
    }
    fb.setTextColor(dCol, COL_BG);
    fb.setTextSize(4);
    int doseW = fb.textWidth(doseBuf);
    fb.drawString(doseBuf, (170 - doseW) / 2, 58);

    // Unit
    fb.setTextColor(COL_DIM, COL_BG);
    fb.setTextSize(2);
    int unitW = fb.textWidth("uSv/h");
    fb.drawString("uSv/h", (170 - unitW) / 2, 98);

    // =====================================================================
    // COUNT RATE (y: 124–148)
    // =====================================================================
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.setTextSize(2);
    fb.drawString("CPS", 8, 128);

    char cpsBuf[16];
    snprintf(cpsBuf, sizeof(cpsBuf), "%.1f", s.countRate);
    fb.setTextColor(COL_VALUE, COL_BG);
    fb.setTextDatum(TR_DATUM);
    fb.drawString(cpsBuf, 164, 128);
    fb.setTextDatum(TL_DATUM);

    fb.drawFastHLine(8, 150, 154, COL_LINE);

    // =====================================================================
    // INFO ROWS (y: 158–250) — size 2 for readability
    // =====================================================================
    fb.setTextSize(2);
    int iy = 158;
    const int rowH = 24;

    // Readings stored
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Stored", 8, iy);
    fb.setTextColor(COL_VALUE, COL_BG);
    fb.setTextDatum(TR_DATUM);
    fb.drawString(String(s.totalReadings), 164, iy);
    fb.setTextDatum(TL_DATUM);
    iy += rowH;

    // Pending upload
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Pending", 8, iy);
    fb.setTextColor(s.pendingReadings > 0 ? COL_YELLOW : COL_GREEN, COL_BG);
    fb.setTextDatum(TR_DATUM);
    fb.drawString(String(s.pendingReadings), 164, iy);
    fb.setTextDatum(TL_DATUM);
    iy += rowH;

    // Last upload
    fb.setTextColor(COL_LABEL, COL_BG);
    fb.drawString("Upload", 8, iy);
    fb.setTextColor(COL_VALUE, COL_BG);
    fb.setTextDatum(TR_DATUM);
    fb.drawString(s.lastUpload, 164, iy);
    fb.setTextDatum(TL_DATUM);
    iy += rowH;

    fb.drawFastHLine(8, iy + 2, 154, COL_LINE);

    // =====================================================================
    // WIFI INFO (bottom)
    // =====================================================================
    iy += 10;
    fb.setTextSize(1);
    if (s.wifiConnected && s.staIP.length() > 0) {
        fb.setTextColor(COL_DIM, COL_BG);
        int ssidW = fb.textWidth(s.wifiSSID);
        fb.drawString(s.wifiSSID, (170 - ssidW) / 2, iy);
        int ipW = fb.textWidth(s.staIP);
        fb.drawString(s.staIP, (170 - ipW) / 2, iy + 12);
    } else {
        fb.setTextColor(COL_DIM, COL_BG);
        int nwW = fb.textWidth("WiFi not connected");
        fb.drawString("WiFi not connected", (170 - nwW) / 2, iy);
    }

    // Push the whole frame at once
    fb.pushSprite(0, 0);
}

#endif // HAS_DISPLAY

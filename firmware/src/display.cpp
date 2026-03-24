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

Display::Display()
    : _tft(TFT_eSPI())
    , _on(false)
    , _wakeTime(0)
    , _lastBtnState(HIGH)
    , _lastDebounce(0)
{
}

void Display::begin() {
    // Enable display power (required for battery operation)
    pinMode(TFT_POWER_PIN, OUTPUT);
    digitalWrite(TFT_POWER_PIN, HIGH);

    // Button with internal pullup
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    _tft.init();
    _tft.setRotation(0);  // Portrait: 170 wide x 320 tall
    _tft.fillScreen(COL_BG);

    // Start with display off (backlight off)
    _sleep();
}

void Display::handleButton() {
    bool reading = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    // Debounce
    if (reading != _lastBtnState) {
        _lastDebounce = now;
    }

    if ((now - _lastDebounce) > DEBOUNCE_MS) {
        // Falling edge: button pressed (active LOW with pullup)
        if (reading == LOW && _lastBtnState == HIGH) {
            if (_on) {
                _sleep();
            } else {
                _wake();
            }
        }
    }

    _lastBtnState = reading;

    // Auto-off timeout
    if (_on && (now - _wakeTime) > DISPLAY_TIMEOUT_MS) {
        _sleep();
    }
}

void Display::_wake() {
    _on = true;
    _wakeTime = millis();
    analogWrite(TFT_BL_PIN, 128);  // ~50% brightness
}

void Display::_sleep() {
    _on = false;
    analogWrite(TFT_BL_PIN, 0);
}

// =============================================================================
// draw() — render the status screen
// =============================================================================

void Display::draw(const DisplayStatus& s) {
    if (!_on) return;

    _tft.fillScreen(COL_BG);
    _tft.setTextDatum(TL_DATUM);

    // ---- Title bar ----
    _tft.setTextColor(COL_TITLE, COL_BG);
    _tft.setTextSize(2);
    _tft.drawString("RadiaLog", 10, 8);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_DIM, COL_BG);
    _tft.drawString("v" FW_VERSION, 130, 14);

    _tft.drawFastHLine(10, 32, 150, COL_LINE);

    // ---- Status indicators ----
    int y = 40;

    // RadiaCode
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.setTextSize(1);
    _tft.drawString("RadiaCode", 10, y);
    _tft.fillCircle(155, y + 4, 5, s.usbConnected ? COL_GREEN : COL_RED);
    y += 18;

    // WiFi
    _tft.drawString("WiFi", 10, y);
    _tft.fillCircle(155, y + 4, 5, s.wifiConnected ? COL_GREEN : COL_RED);
    if (s.wifiConnected && s.wifiSSID.length() > 0) {
        _tft.setTextColor(COL_DIM, COL_BG);
        String ssid = s.wifiSSID.substring(0, 14);
        _tft.drawString(ssid, 40, y);
    }
    y += 18;

    // GPS
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("GPS", 10, y);
    _tft.fillCircle(155, y + 4, 5, s.gpsFix ? COL_GREEN : COL_YELLOW);
    if (s.gpsFix) {
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.drawString(String(s.gpsSats) + " sats", 40, y);
    }
    y += 22;

    _tft.drawFastHLine(10, y, 150, COL_LINE);
    y += 8;

    // ---- Dose Rate (large) ----
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.setTextSize(1);
    _tft.drawString("DOSE RATE", 10, y);
    y += 14;

    _tft.setTextColor(COL_GREEN, COL_BG);
    _tft.setTextSize(3);
    char doseBuf[16];
    snprintf(doseBuf, sizeof(doseBuf), "%.3f", s.doseRate);
    _tft.drawString(doseBuf, 10, y);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("uSv/h", 10, y + 26);
    y += 42;

    // ---- Count Rate ----
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.setTextSize(1);
    _tft.drawString("CPS", 10, y);
    _tft.setTextColor(COL_VALUE, COL_BG);
    char cpsBuf[16];
    snprintf(cpsBuf, sizeof(cpsBuf), "%.1f", s.countRate);
    _tft.drawString(cpsBuf, 50, y);
    y += 18;

    _tft.drawFastHLine(10, y, 150, COL_LINE);
    y += 8;

    // ---- Readings ----
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("Readings", 10, y);
    _tft.setTextColor(COL_VALUE, COL_BG);
    _tft.drawString(String(s.totalReadings), 90, y);
    y += 14;

    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("Pending", 10, y);
    _tft.setTextColor(COL_VALUE, COL_BG);
    _tft.drawString(String(s.pendingReadings), 90, y);
    y += 18;

    // ---- Last Upload ----
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("Last Upload", 10, y);
    y += 14;
    _tft.setTextColor(COL_VALUE, COL_BG);
    _tft.drawString(s.lastUpload, 10, y);
    y += 18;

    _tft.drawFastHLine(10, y, 150, COL_LINE);
    y += 8;

    // ---- Battery bar ----
    _tft.setTextColor(COL_LABEL, COL_BG);
    _tft.drawString("Battery", 10, y);

    if (s.batteryVoltage > 2.0f) {
        char battBuf[16];
        snprintf(battBuf, sizeof(battBuf), "%d%%  %.1fV", s.batteryPercent, s.batteryVoltage);
        _tft.setTextColor(COL_VALUE, COL_BG);
        _tft.drawString(battBuf, 70, y);
        y += 14;

        // Progress bar
        int barW = 150, barH = 8;
        _tft.drawRect(10, y, barW, barH, COL_DIM);
        int fillW = (s.batteryPercent * (barW - 2)) / 100;
        uint16_t barCol = s.batteryPercent > 20 ? COL_GREEN : (s.batteryPercent > 5 ? COL_YELLOW : COL_RED);
        if (fillW > 0) {
            _tft.fillRect(11, y + 1, fillW, barH - 2, barCol);
        }
    } else {
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.drawString("N/A", 70, y);
    }
}

#endif // HAS_DISPLAY

#pragma once
#include "config.h"

#ifdef HAS_DISPLAY

// =============================================================================
// RadiaLog Firmware - Display Graphics Abstraction
// Provides a common type layer over TFT_eSPI (SPI/parallel), LovyanGFX
// (QSPI AMOLED), and Arduino_GFX (CO5300 QSPI).
// =============================================================================

#ifdef DISPLAY_DRIVER_ARDUINO_GFX
// ─────────────────────────────────────────────────────────────────────────────
// Arduino_GFX — for Waveshare ESP32-S3-Touch-AMOLED-1.75-G (CO5300 QSPI)
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino_GFX_Library.h>

// Text datum constants (matching LovyanGFX/TFT_eSPI conventions)
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

class GfxDriver {
    Arduino_ESP32QSPI* _bus;
    Arduino_CO5300*    _gfx;
    uint8_t            _datum;
    uint16_t           _text_color;
    uint16_t           _text_bgcolor;
    uint8_t            _text_size;

    void _applyDatum(const char* str, int16_t x, int16_t y);

public:
    GfxDriver();
    ~GfxDriver();
    bool init();
    void setRotation(uint8_t r);
    void fillScreen(uint16_t color);
    void setTextColor(uint16_t c);
    void setTextColor(uint16_t c, uint16_t bg);
    void setTextDatum(uint8_t d);
    void setTextSize(uint8_t s);
    void drawString(const char* str, int16_t x, int16_t y);
    void drawString(const String& str, int16_t x, int16_t y);
    void setBrightness(uint8_t b);
    int16_t width();
    int16_t height();

    // Low-level shape drawing delegated directly to Arduino_GFX
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    int16_t textWidth(const char* str);
    int16_t textWidth(const String& str);
};

class GfxSprite {
    GfxDriver* _driver;
    uint8_t    _datum;
    uint16_t   _text_color;
    uint16_t   _text_bgcolor;
    uint8_t    _text_size;

public:
    GfxSprite(GfxDriver* driver)
        : _driver(driver)
        , _datum(0)
        , _text_color(0xFFFF)
        , _text_bgcolor(0x0000)
        , _text_size(1)
    {}

    bool createSprite(int16_t w, int16_t h) { return true; } // direct draw
    void fillSprite(uint16_t color) { _driver->fillScreen(color); }
    void setTextColor(uint16_t c) { _text_color = c; _text_bgcolor = c; }
    void setTextColor(uint16_t c, uint16_t bg) { _text_color = c; _text_bgcolor = bg; }
    void setTextDatum(uint8_t d) { _datum = d; }
    void setTextSize(uint8_t s) { _text_size = s; }
    void drawString(const char* str, int16_t x, int16_t y);
    void drawString(const String& str, int16_t x, int16_t y);
    int16_t textWidth(const char* str);
    int16_t textWidth(const String& str);
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { _driver->drawCircle(x, y, r, color); }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { _driver->fillCircle(x, y, r, color); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { _driver->drawRect(x, y, w, h, color); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { _driver->fillRect(x, y, w, h, color); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { _driver->drawFastHLine(x, y, w, color); }
    void pushSprite(int16_t x, int16_t y) {} // no-op: direct draw
};

#else
// ─────────────────────────────────────────────────────────────────────────────
// TFT_eSPI — for standard SPI / parallel displays (ST7789, GC9A01, etc.)
// ─────────────────────────────────────────────────────────────────────────────
#include <TFT_eSPI.h>

typedef TFT_eSPI    GfxDriver;
typedef TFT_eSprite GfxSprite;

#endif // DISPLAY_DRIVER_ARDUINO_GFX

// --- Inline implementations for Arduino_GFX wrapper ------------------------
#ifdef DISPLAY_DRIVER_ARDUINO_GFX

inline GfxDriver::GfxDriver()
    : _bus(nullptr)
    , _gfx(nullptr)
    , _datum(0)
    , _text_color(0xFFFF)
    , _text_bgcolor(0x0000)
    , _text_size(1)
{
}

inline GfxDriver::~GfxDriver() {
    if (_gfx) delete _gfx;
    if (_bus) delete _bus;
}

inline bool GfxDriver::init() {
    _bus = new Arduino_ESP32QSPI(
        12 /* CS */, 38 /* SCK */, 4 /* SDIO0 */, 5 /* SDIO1 */,
        6 /* SDIO2 */, 7 /* SDIO3 */);
    _gfx = new Arduino_CO5300(
        _bus, 39 /* RST */, 0 /* rotation */,
        false /* ips */, 466 /* width */, 466 /* height */,
        0 /* col_offset1 */, 0 /* row_offset1 */,
        0 /* col_offset2 */, 0 /* row_offset2 */);
    if (!_gfx->begin()) {
        return false;
    }
    return true;
}

inline void GfxDriver::setRotation(uint8_t r) {
    if (_gfx) _gfx->setRotation(r);
}

inline void GfxDriver::fillScreen(uint16_t color) {
    if (_gfx) _gfx->fillScreen(color);
}

inline void GfxDriver::setTextColor(uint16_t c) {
    _text_color = c;
    _text_bgcolor = c;
    if (_gfx) _gfx->setTextColor(c);
}

inline void GfxDriver::setTextColor(uint16_t c, uint16_t bg) {
    _text_color = c;
    _text_bgcolor = bg;
    if (_gfx) _gfx->setTextColor(c, bg);
}

inline void GfxDriver::setTextDatum(uint8_t d) {
    _datum = d;
}

inline void GfxDriver::setTextSize(uint8_t s) {
    _text_size = s;
    if (_gfx) _gfx->setTextSize(s);
}

inline void GfxDriver::_applyDatum(const char* str, int16_t x, int16_t y) {
    if (!_gfx) return;
    int16_t x1, y1;
    uint16_t w, h;
    _gfx->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    int16_t cx = x, cy = y;
    switch (_datum) {
        case TC_DATUM: cx = x - x1 - (w / 2); cy = y - y1; break;
        case TR_DATUM: cx = x - x1 - w;       cy = y - y1; break;
        case MC_DATUM: cx = x - x1 - (w / 2); cy = y - y1 - (h / 2); break;
        case BC_DATUM: cx = x - x1 - (w / 2); cy = y - y1 - h; break;
        default:       cx = x - x1;           cy = y - y1; break; // TL_DATUM and others
    }
    _gfx->setCursor(cx, cy);
}

inline void GfxDriver::drawString(const char* str, int16_t x, int16_t y) {
    if (!_gfx) return;
    _gfx->setTextSize(_text_size);
    _gfx->setTextColor(_text_color, _text_bgcolor);
    _applyDatum(str, x, y);
    _gfx->print(str);
}

inline void GfxDriver::drawString(const String& str, int16_t x, int16_t y) {
    drawString(str.c_str(), x, y);
}

inline void GfxDriver::setBrightness(uint8_t b) {
    if (_gfx) _gfx->setBrightness(b);
}

inline int16_t GfxDriver::width() {
    return _gfx ? _gfx->width() : 0;
}

inline int16_t GfxDriver::height() {
    return _gfx ? _gfx->height() : 0;
}

inline void GfxDriver::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (_gfx) _gfx->drawCircle(x, y, r, color);
}

inline void GfxDriver::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (_gfx) _gfx->fillCircle(x, y, r, color);
}

inline void GfxDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_gfx) _gfx->drawRect(x, y, w, h, color);
}

inline void GfxDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_gfx) _gfx->fillRect(x, y, w, h, color);
}

inline void GfxDriver::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (_gfx) _gfx->drawFastHLine(x, y, w, color);
}

inline int16_t GfxDriver::textWidth(const char* str) {
    if (!_gfx) return 0;
    int16_t x1, y1;
    uint16_t w, h;
    _gfx->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
}

inline int16_t GfxDriver::textWidth(const String& str) {
    return textWidth(str.c_str());
}

inline void GfxSprite::drawString(const char* str, int16_t x, int16_t y) {
    if (!_driver) return;
    _driver->setTextSize(_text_size);
    _driver->setTextColor(_text_color, _text_bgcolor);
    _driver->setTextDatum(_datum);
    _driver->drawString(str, x, y);
}

inline void GfxSprite::drawString(const String& str, int16_t x, int16_t y) {
    drawString(str.c_str(), x, y);
}

inline int16_t GfxSprite::textWidth(const char* str) {
    if (!_driver) return 0;
    _driver->setTextSize(_text_size);
    return _driver->textWidth(str);
}

inline int16_t GfxSprite::textWidth(const String& str) {
    return textWidth(str.c_str());
}

#endif // DISPLAY_DRIVER_ARDUINO_GFX
// ---------------------------------------------------------------------------

#endif // HAS_DISPLAY

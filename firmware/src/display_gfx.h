#pragma once
#include "config.h"

#ifdef HAS_DISPLAY

// =============================================================================
// RadiaLog Firmware - Display Graphics Abstraction
// Provides a common type layer over TFT_eSPI (SPI/parallel) and
// LovyanGFX (QSPI AMOLED).  Both libraries share a nearly identical
// drawing API, so display.cpp works with either via these typedefs.
// =============================================================================

#ifdef DISPLAY_DRIVER_LGFX
// ─────────────────────────────────────────────────────────────────────────────
// LovyanGFX — for QSPI AMOLED displays (CO5300, SH8601, RM67162, etc.)
// ─────────────────────────────────────────────────────────────────────────────
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// --- Board-specific panel configuration ------------------------------------
#if defined(BOARD_WS_AMOLED_175)

// Waveshare ESP32-S3-Touch-AMOLED-1.75-G
// 466x466 CO5300 QSPI AMOLED, circular
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_QSPI      _bus_instance;
    lgfx::Panel_SH8601Z _panel_instance;   // CO5300 is SH8601-compatible
    lgfx::Light_PWM      _light_instance;

public:
    LGFX(void) {
        // --- QSPI bus ---
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host  = SPI2_HOST;
            cfg.freq_write = 40000000;
            cfg.pin_sclk  = 38;
            cfg.pin_io0   = 4;
            cfg.pin_io1   = 5;
            cfg.pin_io2   = 6;
            cfg.pin_io3   = 7;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // --- Panel ---
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs          = 12;
            cfg.pin_rst         = 39;
            cfg.panel_width     = 466;
            cfg.panel_height    = 466;
            cfg.memory_width    = 466;
            cfg.memory_height   = 466;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;
            cfg.invert          = false;
            cfg.rgb_order       = false;
            cfg.bus_shared      = false;
            _panel_instance.config(cfg);
        }

        // --- Backlight (PWM via CO5300 register; fallback to PWM pin) ---
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl   = 47;      // BL pin on this board
            cfg.invert   = false;
            cfg.freq     = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif // BOARD_WS_AMOLED_175

// Type aliases for the rest of the codebase
typedef LGFX              GfxDriver;
typedef lgfx::LGFX_Sprite GfxSprite;

#else
// ─────────────────────────────────────────────────────────────────────────────
// TFT_eSPI — for standard SPI / parallel displays (ST7789, GC9A01, etc.)
// ─────────────────────────────────────────────────────────────────────────────
#include <TFT_eSPI.h>

typedef TFT_eSPI    GfxDriver;
typedef TFT_eSprite GfxSprite;

#endif // DISPLAY_DRIVER_LGFX

#endif // HAS_DISPLAY

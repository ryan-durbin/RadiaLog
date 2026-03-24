#include "battery.h"

// =============================================================================
// RadiaLog Firmware - Battery Voltage Monitor Implementation
// Uses analogReadMilliVolts() for calibrated ADC reading with oversampling.
// =============================================================================

Battery::Battery()
    : _voltage(0.0f)
{
}

void Battery::begin() {
    analogSetAttenuation(ADC_11db);
    pinMode(BATTERY_ADC_PIN, INPUT);
}

void Battery::update() {
    // Oversample for noise reduction
    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogReadMilliVolts(BATTERY_ADC_PIN);
    }
    float adc_mv = static_cast<float>(sum) / BATTERY_SAMPLES;

    // Apply voltage divider ratio to get actual battery voltage
    _voltage = (adc_mv * BATTERY_DIVIDER_RATIO) / 1000.0f;
}

uint8_t Battery::getPercent() const {
    if (_voltage <= 0.0f) return 0;

    uint16_t mv = static_cast<uint16_t>(_voltage * 1000.0f);

    if (mv >= BATTERY_FULL_MV) return 100;
    if (mv <= BATTERY_EMPTY_MV) return 0;

    return static_cast<uint8_t>(
        (static_cast<uint32_t>(mv - BATTERY_EMPTY_MV) * 100) /
        (BATTERY_FULL_MV - BATTERY_EMPTY_MV)
    );
}

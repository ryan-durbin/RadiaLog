#include "battery_axp2101.h"
#include "config.h"

#ifdef BATTERY_TYPE_AXP2101

#include <XPowersLib.h>

// =============================================================================
// RadiaLog Firmware - AXP2101 PMU Battery Monitor Implementation
// Reads battery state via XPowersLib from the AXP2101 over I2C.
// =============================================================================

BatteryAXP2101::BatteryAXP2101()
    : _pmu(nullptr)
    , _voltage(0.0f)
    , _percent(0)
    , _present(false)
    , _charging(false)
{
}

BatteryAXP2101::~BatteryAXP2101() {
    delete _pmu;
}

void BatteryAXP2101::begin(TwoWire& wire, int sdaPin, int sclPin) {
    _pmu = new XPowersAXP2101();
    if (!_pmu->begin(wire, AXP2101_SLAVE_ADDRESS, sdaPin, sclPin)) {
        Serial.println(F("[Battery] AXP2101 init failed!"));
        delete _pmu;
        _pmu = nullptr;
        return;
    }

    // Enable battery voltage ADC and coulomb counter
    _pmu->enableBattVoltageMeasure();
    _pmu->enableBattDetection();

#ifdef BOARD_WS_AMOLED_175
    // Enable essential power rails for display panel (CO5300)
    if (_pmu->enableDC1()) {
        Serial.println(F("[Battery] DC1 enabled (display VCC 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: DC1 enable failed"));
    }

    // Enable ALDO1 for display backlight/panel logic
    if (_pmu->enableALDO1()) {
        Serial.println(F("[Battery] ALDO1 enabled (backlight VCC 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: ALDO1 enable failed"));
    }

    // Enable BLDO1 for touch IC power
    if (_pmu->enableBLDO1()) {
        Serial.println(F("[Battery] BLDO1 enabled (touch VCC 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: BLDO1 enable failed"));
    }

    // Enable DCDC2 for panel VCI power
    if (_pmu->enableDC2()) {
        Serial.println(F("[Battery] DC2 enabled (panel VCI 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: DC2 enable failed"));
    }

    // Enable DCDC4 for GPS power
    if (_pmu->enableDC4()) {
        Serial.println(F("[Battery] DC4 enabled (GPS VCC 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: DC4 enable failed"));
    }

    // Enable ALDO2 for SD card power
    if (_pmu->enableALDO2()) {
        Serial.println(F("[Battery] ALDO2 enabled (SD VCC 3.3V)"));
    } else {
        Serial.println(F("[Battery] WARNING: ALDO2 enable failed"));
    }

    // Enable CPUSLDO for additional logic power
    if (_pmu->enableCPUSLDO()) {
        Serial.println(F("[Battery] CPUSLDO enabled"));
    } else {
        Serial.println(F("[Battery] WARNING: CPUSLDO enable failed"));
    }

#endif // BOARD_WS_AMOLED_175

    Serial.println(F("[Battery] AXP2101 initialized."));
}

void BatteryAXP2101::update() {
    if (!_pmu) return;

    _present  = _pmu->isBatteryConnect();
    _charging = _pmu->isCharging();

    if (_present) {
        _voltage = _pmu->getBattVoltage() / 1000.0f;  // mV → V
        _percent = static_cast<uint8_t>(_pmu->getBatteryPercent());
        if (_percent > 100) _percent = 100;
    } else {
        _voltage = 0.0f;
        _percent = 0;
    }
}

#endif // BATTERY_TYPE_AXP2101

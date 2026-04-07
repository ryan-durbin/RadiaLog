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

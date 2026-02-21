/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Proximity Pilot (PP) detection class. Reads the PP pin to determine
 *              the maximum current capacity of the connected EV cable. This feature
 *              can be disabled via the web UI, in which case the user-configured
 *              maximum current is used as the hardware limit.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * 
 * 
 * --- PP (Proximity Pilot) Resistor Values (IEC 61851-1 / IEC 62196) ---
 *   Resistor      | Max Current | Cable Rating
 *   --------------|-------------|----------------
 *   100Ω          | 63A         | Heavy duty
 *   220Ω          | 32A         | Common Type 2
 *   680Ω          | 20A         | Medium duty
 *   1500Ω (1.5kΩ) | 13A         | Light duty
 *
 * The EVSE reads the voltage drop across these resistors to determine the cable's
 * capacity and limit charging current accordingly to prevent overheating.
 * ========================================================================================= */

#ifndef PROXIMITY_H
#define PROXIMITY_H

#include <Arduino.h>
#include "EvseConfig.h"

class EvseCharge;

// =========================
// Hardware Pin Configuration
// =========================
// Define the GPIO pin used for Proximity Pilot (PP) sensing.
// This must be an ADC-capable pin.
constexpr int PIN_PROXIMITY_IN = 39; // Default: GPIO39 (ADC1_CH3 on ESP32)

// Proximity Pilot (PP) voltage thresholds in millivolts (mV).
// These values correspond to the resistance in the EV cable plug, which indicates
// the cable's maximum current capacity. Higher resistance = higher voltage = lower current.
constexpr int PROXIMITY_VOLTAGE_MV_NO_CABLE = 3000; // Open circuit / No cable connected
constexpr int PROXIMITY_VOLTAGE_MV_13A = 1800; // Threshold for 13A cable (e.g., 1.5kOhm resistor)
constexpr int PROXIMITY_VOLTAGE_MV_20A = 1200; // Threshold for 20A cable (e.g., 680 Ohm resistor)
constexpr int PROXIMITY_VOLTAGE_MV_32A = 550;  // Threshold for 32A cable (e.g., 220 Ohm resistor)
constexpr int PROXIMITY_VOLTAGE_MV_63A = 280;  // Threshold for 63A cable (e.g., 100 Ohm resistor)
constexpr int PROXIMITY_VOLTAGE_MV_SHORT = 200;  // Short circuit - ERROR condition

class Proximity {
public:
    Proximity(EvseCharge& evse, AppConfig& config);
    ~Proximity() = default;

    bool begin();
    void loop();

private:
    uint8_t getMaxCurrent();

    bool _initialized;
    EvseCharge* _evse;
    AppConfig* _config;
    unsigned long _lastCheck;
};

#endif // PROXIMITY_H
/* =========================================================================================
 * Project:     Evse-SyncCharge
 * File:        Proximity.cpp
 * Description: Implementation of the Proximity Pilot (PP) detection class.
 *
 * The Proximity Pilot is a safety mechanism defined in IEC 61851-1 and IEC 62196 standards.
 * It uses a resistor embedded in the EV charging cable plug to communicate the cable's
 * maximum current-carrying capacity to the EVSE (Electric Vehicle Supply Equipment).
 *
 * How It Works:
 * -------------
 * The EVSE applies a voltage to the PP pin through a pull-up resistor. The cable's internal
 * resistor forms a voltage divider, and the EVSE reads the resulting voltage via ADC.
 * Higher resistance in the cable = higher voltage reading = lower current capacity.
 *
 * Standard PP Resistor Values (IEC 61851-1 / IEC 62196):
 * ------------------------------------------------------
 *   Resistor     | Max Current | Typical Use
 *   -------------|-------------|------------------
 *   100 Ohm      | 63A         | Heavy-duty cables
 *   220 Ohm      | 32A         | Standard Type 2 cables
 *   680 Ohm      | 20A         | Medium-duty cables
 *   1500 Ohm     | 13A         | Light-duty cables
 *   4700 Ohm     | N/A         | No cable / plug button released
 *   Open circuit | N/A         | Cable not connected or PP wire fault
 *   Short (0 Ohm)| ERROR       | Fault condition - do not charge
 *
 * Safety Behavior:
 * ----------------
 * This class ensures the EVSE never exceeds the cable's rated current by:
 *   1. Periodically reading the PP voltage via calibrated ADC
 *   2. Mapping the voltage to the corresponding current limit
 *   3. Taking the MINIMUM of the cable rating and user-configured maximum
 *   4. Updating the EvseCharge controller with this effective hardware limit
 *
 * If no cable is detected (open circuit), or if a cable fault is detected (short circuit),
 * the module reports a safe value that results in the user-configured max current being used.
 *
 * If PP sensing is disabled entirely via the `sensePpEnabled` flag in the web UI,
 * the user-configured `maxCurrent` is passed directly to the EvseCharge controller.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * ========================================================================================= */

#include "Proximity.h"
#include "EvseCharge.h"
#include "EvseLogger.h" // Assumes access to the project's logger

Proximity::Proximity(EvseCharge& evse, AppConfig& config) :
    _evse(&evse),
    _config(&config),
    _initialized(false),
    _lastVoltageMv(0),
    _lastCheck(0)
{}

bool Proximity::begin() {
    // Standard Arduino ADC Setup
    analogSetPinAttenuation(PIN_PROXIMITY_IN, ADC_11db);
    pinMode(PIN_PROXIMITY_IN, INPUT);
    _initialized = true;
    logger.infof("[PROXIMITY] Initialized on GPIO %d.", PIN_PROXIMITY_IN);
    return true;
}

uint8_t Proximity::getMaxCurrent() {
    if (!_initialized) {
        logger.warn("[PROXIMITY] Not initialized, cannot read current.");
        return 0; // Return a safe/default value
    }

    // Read directly using standard Arduino API
    int voltage_mv = analogReadMilliVolts(PIN_PROXIMITY_IN);
    _lastVoltageMv = voltage_mv;

    logger.infof("[PROXIMITY] Measured: %dmV", voltage_mv);

    uint8_t current;
    if (voltage_mv < PROXIMITY_VOLTAGE_MV_SHORT) {
        // Short circuit detected - ERROR condition
        logger.error("[PROXIMITY] PP SHORT CIRCUIT DETECTED! Cable fault.");
        return 0; // Return 0 to indicate error - will use user's maxCurrent as fallback
    } else if (voltage_mv >= PROXIMITY_VOLTAGE_MV_NO_CABLE) {
        // Open circuit / No cable detected - return 0 to indicate no PP signal
        logger.debug("[PROXIMITY] No cable detected (open circuit)");
        return 0;
    } else if (voltage_mv >= PROXIMITY_VOLTAGE_MV_13A) {
        current = 13;
    } else if (voltage_mv >= PROXIMITY_VOLTAGE_MV_20A) {
        current = 20;
    } else if (voltage_mv >= PROXIMITY_VOLTAGE_MV_32A) {
        current = 32;
    } else if (voltage_mv >= PROXIMITY_VOLTAGE_MV_63A) {
        current = 63; // 100 Ohm cable (400-800mV range)
    } else {
        // Between SHORT and 63A threshold - treat as 63A cable
        current = 63;
    }

    logger.infof("[PROXIMITY] Max cable current: %dA", current);

    return current;
}

int Proximity::getLastVoltageMv() const {
    return _lastVoltageMv;
}

void Proximity::loop() {
    // Periodically check cable current limit
    // We use a simple timer to avoid reading the ADC on every single loop cycle.
    if (millis() - _lastCheck > 1000) { // Check every 1 second (Debug)
        _lastCheck = millis();
        
        // Start with hardware-configured max as the ceiling
        uint8_t effectiveLimit = (uint8_t)_config->maxCurrent;
        
        if (_config->sensePpEnabled) {
            // Read the cable's max current from PP sensor
            uint8_t cableCurrent = getMaxCurrent();
            // cableCurrent == 0 means no cable detected -> use user's maxCurrent
            // Otherwise, use the MINIMUM of cable sense and hardware config for safety
            if (cableCurrent > 0 && cableCurrent < effectiveLimit) {
                effectiveLimit = cableCurrent;
            }
            // If cableCurrent is 0 (no cable), effectiveLimit remains at user's maxCurrent
        }
        
        _evse->setHardwareCurrentLimit(effectiveLimit);
    }
}
/*****************************************************************************
 * @file Pilot.h
 * @brief Pilot driver API and vehicle state helpers.
 *
 * @details
 * Declares the `Pilot` class responsible for PWM generation on the pilot
 * pin and voltage sensing to derive vehicle connection/state information.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "EvseTypes.h" // For VEHICLE_STATE_T

// =========================
// Compile-time Configuration
// =========================

// =========================
// Constants
// =========================
// Current limits (SAE J1772)
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 64.0f;

constexpr float PILOT_VOLTAGE_SCALE = ((1200.0+3300.0)/1200.0); // Voltage divider scale factor

// Voltage thresholds in millivolts (SAE J1772 state detection)
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 11000;    // >= 11V = Not connected
constexpr int VOLTAGE_STATE_CONNECTED = 8000;          // >= 8V  = Connected, not ready
constexpr int VOLTAGE_STATE_READY = 5000;              // >= 5V  = Ready
constexpr int VOLTAGE_STATE_VENTILATION = 2000;        // >= 2V  = Ready, ventilation required
constexpr int VOLTAGE_STATE_NO_POWER = 0;              // >= 0V  = No power

// =========================
// Pilot class
// =========================
class Pilot {
private:
    float currentDutyPercent = 0.0f;
    bool pwmAttached = false;         // track if PWM is currently attached
    VEHICLE_STATE_T lastVehicleState = VEHICLE_STATE_COUNT;  // track last state for change detection
public:
    Pilot();
    void begin();
    void standby();
    void disable();
    void currentLimit(float amps);
    int readPin();
    int analogReadMax();
    float convertMv(int pinValueMv);
    float getVoltage();
    VEHICLE_STATE_T read();
    float getPwmDuty();
};

// =========================
// Helper function
// =========================
void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif

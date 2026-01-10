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
// Constants - OFFICIAL SAE J1772 VALUES (2026)
// =========================

constexpr float PILOT_VOLTAGE_SCALE = ((1200.0+3300.0)/1200.0)*1.2; // Voltage divider scale factor

// Current limits (SAE J1772)
constexpr float MIN_CURRENT = 6.0f;                    // Absolute minimum allowed
constexpr float MAX_CURRENT = 80.0f;                   // Official maximum continuous current

// J1772 PWM Conversion Constants (exact spec values)
constexpr float J1772_MIN_DUTY          = 10.0f;       // 6 A minimum
constexpr float J1772_MAX_DUTY          = 96.0f;       // 80 A maximum

constexpr float J1772_LOW_RANGE_MAX_AMPS   = 51.0f;    // Boundary between low & high formula
constexpr float J1772_LOW_RANGE_MAX_DUTY   = 85.0f;    // 51 A = 85% × 0.6
constexpr float J1772_LOW_RANGE_FACTOR     = 0.6f;     // Amps per % duty (10–85%)

constexpr float J1772_HIGH_RANGE_FACTOR    = 2.5f;     // Amps per % duty (85–96%)
constexpr float J1772_HIGH_RANGE_OFFSET    = 64.0f;    // Offset for high range formula

// Voltage thresholds in millivolts (SAE J1772 state detection)
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 11000;     // >= 11V = Not connected (State A)
constexpr int VOLTAGE_STATE_CONNECTED     =  8000;     // >= 8V  = Connected, not ready (State B)
constexpr int VOLTAGE_STATE_READY         =  5000;     // >= 5V  = Ready (State C/D)
constexpr int VOLTAGE_STATE_VENTILATION   =  2000;     // >= 2V  = Ready, ventilation required (State D)
constexpr int VOLTAGE_STATE_NO_POWER      =     0;     // >= 0V  = No power / Fault

// =========================
// Pilot class
// =========================
class Pilot {
private:    
    int voltageMv = 0;
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
    float getVoltage();
    VEHICLE_STATE_T read();
    float getPwmDuty();
    float ampsToDuty(float amps);
    float dutyToAmps(float duty);
private:
    float convertMv(int adMv);
};

// =========================
// Helper function
// =========================
void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif
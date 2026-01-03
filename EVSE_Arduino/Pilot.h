/*****************************************************************************
 * @file Pilot.h
 * @brief Pilot driver API and vehicle state helpers.
 *
 * @details
 * Declares the `Pilot` class responsible for PWM generation on the pilot
 * pin and voltage sensing to derive vehicle connection/state information.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "EvseTypes.h" // For VEHICLE_STATE_T

// =========================
// Constants
// =========================
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 80.0f;

// =========================
// Pilot class
// =========================
class Pilot {
private:
    float voltage = 0.0f;
    float currentDutyPercent = 0.0f;
    bool pwmAttached = false;         // track if PWM is currently attached
    VEHICLE_STATE_T lastVehicleState = VEHICLE_STATE_COUNT;  // track last state for change detection
public:
    Pilot();
    void standby();
    void disable();
    void currentLimit(float amps);
    float readPin();
    float getVoltage();
    VEHICLE_STATE_T read();
    float getPwmDuty();
};

// =========================
// Helper function
// =========================
void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif

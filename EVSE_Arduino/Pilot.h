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
// #define USE_PILOT_IRQ  // Uncomment to use 20kHz ISR sampling (independent of loop)
                           // Comment out to use original loop-based sampling

// =========================
// Constants
// =========================
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 64.0f;

// Voltage conversion calibration
constexpr float PILOT_VOLTAGE_SCALE = 8.0/12.0;
constexpr float PILOT_VOLTAGE_OFFSET = 0;

// ADC conversion constants
//constexpr float ADC_VREF = 3.3f;
constexpr float ADC_VREF = 12.0;
constexpr int ADC_MAX_VALUE = 4095;

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
#ifdef USE_PILOT_IRQ
    void initPilotIrq();  // Initialize 20kHz timer ISR
    void deinitPilotIrq(); // Cleanup timer
#endif
};

// =========================
// Helper function
// =========================
void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif

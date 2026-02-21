/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the J1772 Pilot state machine. Determines vehicle
 *              state based on voltage data from the AdcManager.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <esp32-hal-ledc.h>
#include <limits.h> // Added for INT_MAX

#include "EvseLogger.h"
#include "Pilot.h"

// Constructor - Clean and empty because variables are initialized in the header
Pilot::Pilot()
{
}

Pilot::~Pilot()
{
}

void Pilot::begin()
{
    // Standard Arduino ADC Setup
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_PILOT_IN, ADC_11db);
    pinMode(PIN_PILOT_IN, INPUT);
    logger.info("[PILOT] Initialized (Standard ADC)");
}

void Pilot::standby()
{
    // Pre-set GPIO to HIGH to prevent glitch to 0V (VEHICLE_NO_POWER) during detach
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);

    if(pwmAttached) {
        logger.info("[PILOT] Detaching PWM for Standby (Static HIGH)");
        pwmAttached = false;
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
}

void Pilot::disable()
{
    standby();
}

void Pilot::stop()
{
    standby(); // Force PWM to 12V (Safety)
    // AdcManager::stop() should be called separately during shutdown sequence
}

void Pilot::currentLimit(float amps)
{
    float dutyPercent = ampsToDuty(amps);

    // Prevent log flooding: only update if value changed or PWM was off
    if (pwmAttached && fabs(currentDutyPercent - dutyPercent) < 0.05f) {
        return;
    }

    currentDutyPercent = dutyPercent;

    uint32_t dutyCounts = (uint32_t)roundf((dutyPercent / 100.0f) * PILOT_PWM_MAX_DUTY);

    if(!pwmAttached) {
        logger.infof("[PILOT] PWM Enabled: %.2f A (Duty: %.1f%%)", amps, dutyPercent);
        ledcAttach(PIN_PILOT_PWM_OUT, PILOT_PWM_FREQ, PILOT_PWM_RESOLUTION);
        pwmAttached = true;
    } else {
        logger.infof("[PILOT] PWM Adjusted: %.2f A (Duty: %.1f%%)", amps, dutyPercent);
    }
    ledcWrite(PIN_PILOT_PWM_OUT, dutyCounts);
}



VEHICLE_STATE_T Pilot::read() {
    int highRaw = 0;
    int lowRaw = 5000; // Start higher than max possible 3.3V (3300mV)

    // Sample for 2 full PWM periods (approx 2ms) to catch the peaks
    unsigned long startTime = micros();
    while ((micros() - startTime) < PILOT_SAMPLE_DURATION_US) {
        int val = analogReadMilliVolts(PIN_PILOT_IN);
        if (val > highRaw) highRaw = val;
        if (val < lowRaw)  lowRaw = val;
    }
    
    // 2. Conversion
    highVoltageMv = (int)convertMv(highRaw);
    lowVoltageMv  = (int)convertMv(lowRaw);

    // 3. Temporary state determination
    VEHICLE_STATE_T detectedState;
    if (highVoltageMv >= VOLTAGE_STATE_NOT_CONNECTED)      detectedState = VEHICLE_NOT_CONNECTED;
    else if (highVoltageMv >= VOLTAGE_STATE_CONNECTED)     detectedState = VEHICLE_CONNECTED;
    else if (highVoltageMv >= VOLTAGE_STATE_READY)         detectedState = VEHICLE_READY;
    else if (highVoltageMv >= VOLTAGE_STATE_VENTILATION)   detectedState = VEHICLE_READY_VENTILATION_REQUIRED;
    else                                                   detectedState = VEHICLE_NO_POWER;

    // Diode Check (State F)
    if (pwmAttached && detectedState != VEHICLE_NOT_CONNECTED) {
        if (lowVoltageMv > VOLTAGE_STATE_N12V_THRESHOLD) {
            detectedState = VEHICLE_ERROR;
        }
    }

    // 4. "Best of 3" Debouncing
    // We only update lastVehicleState if we see the same detectedState multiple times
    static VEHICLE_STATE_T candidateState = VEHICLE_ERROR;
    static int stabilityCounter = 0;

    if (detectedState == candidateState) {
        stabilityCounter++;
    } else {
        candidateState = detectedState;
        stabilityCounter = 0;
    }

    // Only commit to the change if it has been stable for 3 checks
    if (stabilityCounter >= 3 && candidateState != lastVehicleState) {
        lastVehicleState = candidateState;
        
        char stateBuf[50];
        vehicleStateToText(lastVehicleState, stateBuf);
        logger.debugf("[PILOT] Stable Change: %s (H:%dmV L:%dmV)", 
                      stateBuf, highVoltageMv, lowVoltageMv);
    }

    return lastVehicleState;
}

/* API & Helper Methods */
float Pilot::getVoltage() { return (float)highVoltageMv / 1000.0f; }
float Pilot::getPwmDuty() { return currentDutyPercent; }
float Pilot::convertMv(int adMv) {
    // Formula: V = V_min + (ADV - ADV_min) * scale
    // Returns Pilot Voltage in Millivolts
    return (float)CAL_PILOT_MV_LOW + ((float)(adMv - CAL_ADC_MV_LOW) * PILOT_CAL_SLOPE);
}

float Pilot::ampsToDuty(float amps) {
    amps = constrain(amps, MIN_CURRENT, MAX_CURRENT);
    if (amps <= J1772_LOW_RANGE_MAX_AMPS) return amps / J1772_LOW_RANGE_FACTOR;
    return (amps / J1772_HIGH_RANGE_FACTOR) + J1772_HIGH_RANGE_OFFSET;
}

float Pilot::dutyToAmps(float duty) {
    if (duty <= J1772_LOW_RANGE_MAX_DUTY) return duty * J1772_LOW_RANGE_FACTOR;
    return (duty - J1772_HIGH_RANGE_OFFSET) * J1772_HIGH_RANGE_FACTOR;
}

// Keep this for legacy calls if necessary, but logic is now inside read()
int Pilot::analogReadMax() { return highVoltageMv; }

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer)
{
    switch (vehicleState) {
        case VEHICLE_NOT_CONNECTED:              strcpy(buffer, "A: Standby"); break;
        case VEHICLE_CONNECTED:                  strcpy(buffer, "B: Vehicle Detected"); break;
        case VEHICLE_READY:                      strcpy(buffer, "C: Charging"); break;
        case VEHICLE_READY_VENTILATION_REQUIRED: strcpy(buffer, "D: Ventilation Req"); break;
        case VEHICLE_NO_POWER:                   strcpy(buffer, "E: No Power"); break;
        case VEHICLE_ERROR:                      strcpy(buffer, "F: Fault/Error"); break;
        default:                                 strcpy(buffer, "Unknown"); break;
    }
}
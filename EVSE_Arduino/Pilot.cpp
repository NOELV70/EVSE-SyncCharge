/*****************************************************************************
 * @file Pilot.cpp
 * @brief Pilot signal driver for SAE J1772 PWM and vehicle state sensing.
 *
 * @details
 * Controls PWM on the pilot pin, reads pilot voltage for vehicle state
 * detection and exposes convenience helpers used by the EVSE charging logic.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#include <Arduino.h>
#include <cstring>
#include <cmath>

#include <esp32-hal-ledc.h>

#include "EvseLogger.h"
#include "Pilot.h"

/* =========================
 * PWM Configuration
 * ========================= */
constexpr int PIN_PILOT_PWM_OUT    = 27;   // PWM output
constexpr int PILOT_PWM_FREQ       = 1000; // 1 kHz
constexpr int PILOT_PWM_RESOLUTION = 12;   // 12-bit (0–4095)
constexpr int PILOT_PWM_CHANNEL = 0; // LEDC channel (kept for compatibility)

/* =========================
 * Analog Sampling Configuration
 * ========================= */
constexpr unsigned long SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ; // Sample for 2 PWM periods

/* =========================
 * Analog input pin
 * ========================= */
constexpr int PIN_PILOT_IN = 36; // ADC1 channel 0

/* =========================
 * Helper function: analog read max
 * ========================= */
int Pilot::analogReadMax()
{
   int maxVal = 0;
   // Sample for exactly 2 PWM periods to capture peak reliably
   unsigned long startTime = micros();
   unsigned long duration = 0;
   
   while (duration < SAMPLE_DURATION_US) {
       int val = analogReadMilliVolts(PIN_PILOT_IN);
       if (val > maxVal) maxVal = val;
       duration = micros() - startTime;
   }
   //logger.debugf("[PILOT] analogReadMax sampled for %lu us, result: %d", duration, maxVal);
   return maxVal;
}

// Constructor
Pilot::Pilot() 
{
}

void Pilot::disable()
{
    logger.debug("[PILOT] Setting pilot disable"); 
    standby();
}

void Pilot::begin()
{
    // Explicit ADC configuration (ESP32)
    analogReadResolution(12);                       // 0–4095
    analogSetPinAttenuation(PIN_PILOT_IN, ADC_11db); // ~0–3.9V range

    pinMode(PIN_PILOT_IN, INPUT);

    logger.info("[PILOT] ADC configured: 12-bit, 11dB attenuation");
}

void Pilot::standby()
{
    if(pwmAttached)
    {
        logger.debug("[PILOT] Setting pilot HIGH (standby)"); /* only print if we change to standby mode !*/
        pwmAttached=false;
        // Stop PWM on this pin (toolchain provides ledcDetach)
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
    // Set pin as HIGH output
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);
}

float Pilot::ampsToDuty(float amps)
{
    float dutyPercent;
    if (amps <= J1772_LOW_RANGE_MAX_AMPS)
        dutyPercent = amps / J1772_LOW_RANGE_FACTOR;
    else
        dutyPercent = (amps / J1772_HIGH_RANGE_FACTOR) + J1772_HIGH_RANGE_OFFSET;

    return constrain(dutyPercent, 0.0f, 100.0f);
}

float Pilot::dutyToAmps(float duty)
{
    if (duty <= J1772_LOW_RANGE_MAX_DUTY) return duty * J1772_LOW_RANGE_FACTOR;
    else return (duty - J1772_HIGH_RANGE_OFFSET) * J1772_HIGH_RANGE_FACTOR;
}

void Pilot::currentLimit(float amps)
{
    amps = constrain(amps, MIN_CURRENT, MAX_CURRENT);
    float dutyPercent = ampsToDuty(amps);

    currentDutyPercent = dutyPercent; // <-- store duty percent

    uint32_t maxDuty = (1UL << PILOT_PWM_RESOLUTION) - 1;
    uint32_t dutyCounts = (uint32_t)roundf((dutyPercent / 100.0f) * maxDuty);

    logger.infof("[PILOT] Pilot current: %.2f A | Duty: %.2f %% | Counts: %lu", amps, dutyPercent, dutyCounts);

    // ESP32 PWM setup
    if(!pwmAttached)
    {
        // Some ESP32 Arduino cores expose pin-based ledcAttach/ledcDetach API.
        // Use ledcAttach(pin, freq, resolution) if available in this toolchain.
        ledcAttach(PIN_PILOT_PWM_OUT, PILOT_PWM_FREQ, PILOT_PWM_RESOLUTION);
        pwmAttached=true;
    }
    // Use pin-based ledcWrite when channel-based API isn't available.
    ledcWrite(PIN_PILOT_PWM_OUT, dutyCounts);
}

int Pilot::readPin()
{
    // Loop mode: sample on-demand during 2 PWM periods
    // analogReadMilliVolts() handles ADC conversion automatically (ESP32 boards core)
    int pinValueMv = analogReadMilliVolts(PIN_PILOT_IN);
    // Store as float in millivolts (no division by 1000)
    return pinValueMv;
}

float Pilot::convertMv(int adMv)
{
    float voltageMv = (((float)(adMv) * PILOT_VOLTAGE_SCALE));
    //logger.debugf("[PILOT] Analog: pinValueMv=%d(unscaled) Voltage=%f mV", adMv, voltageMv);
    return voltageMv;
}

float Pilot::getVoltage()
{
    return (voltageMv/1000.0);
}

VEHICLE_STATE_T Pilot::read()
{
    VEHICLE_STATE_T state;
    int mvUnscaled = analogReadMax();  // Now in millivolts UNSCALED 
    voltageMv=(int) convertMv(mvUnscaled);    

    if (voltageMv >= VOLTAGE_STATE_NOT_CONNECTED)       state = VEHICLE_NOT_CONNECTED;
    else if (voltageMv >= VOLTAGE_STATE_CONNECTED)      state = VEHICLE_CONNECTED;
    else if (voltageMv >= VOLTAGE_STATE_READY)          state = VEHICLE_READY;
    else if (voltageMv >= VOLTAGE_STATE_VENTILATION)    state = VEHICLE_READY_VENTILATION_REQUIRED;
    else if (voltageMv >= VOLTAGE_STATE_NO_POWER)       state = VEHICLE_NO_POWER;
    else state = VEHICLE_ERROR;
    
    // Log only on state change
    if (state != lastVehicleState) {
        lastVehicleState = state;
        char stateBuf[50];
        vehicleStateToText(state, stateBuf);
        logger.debugf("[PILOT] Read: voltage=%d mV -> %.2f -> %s", voltageMv, getVoltage(), stateBuf);
    }
    
    return state;
}

float Pilot::getPwmDuty() {
    //logger.debugf("[PILOT] getPwmDuty -> %.2f %%", currentDutyPercent);
    return currentDutyPercent; // store last duty calculated in currentLimit()
}

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer)
{
    switch (vehicleState)
    {
        case VEHICLE_NOT_CONNECTED:              strcpy(buffer, "Not connected"); break;
        case VEHICLE_CONNECTED:                  strcpy(buffer, "Connected, not ready"); break;
        case VEHICLE_READY:                      strcpy(buffer, "Ready"); break;
        case VEHICLE_READY_VENTILATION_REQUIRED: strcpy(buffer, "Ready, ventilation required"); break;
        case VEHICLE_NO_POWER:                   strcpy(buffer, "No power"); break;
        case VEHICLE_ERROR:                      strcpy(buffer, "Error"); break;
        default:                                 strcpy(buffer, "Unknown"); break;
    }
}
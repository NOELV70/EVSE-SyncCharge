/*****************************************************************************
 * @file Pilot.cpp
 * @brief Pilot signal driver for SAE J1772 PWM and vehicle state sensing.
 *
 * @details
 * Controls PWM on the pilot pin, reads pilot voltage for vehicle state
 * detection and exposes convenience helpers used by the EVSE charging logic.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
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
constexpr int PIN_PILOT_PWM_OUT        = 27;   // PWM output
constexpr int PILOT_PWM_FREQ       = 1000; // 1 kHz
constexpr int PILOT_PWM_RESOLUTION = 12;   // 12-bit (0–4095)
constexpr int PILOT_PWM_CHANNEL = 0; // LEDC channel (kept for compatibility)

/* =========================
 * Analog input pin
 * ========================= */
constexpr int PIN_PILOT_IN = 36; // ADC1 channel 0

/* =========================
 * Helper function: analog read max
 * ========================= */
int analogReadMax(uint8_t pinNumber, uint8_t count)
{
    int maxVal = 0;
    for (int i = 0; i < count; i++)
    {
        int val = analogRead(pinNumber);
        if (val > maxVal) maxVal = val;
    }
    return maxVal;
}
// Constructor
Pilot::Pilot() 
{
    voltage = 0.0f;
    currentDutyPercent = 0.0f;
}

void Pilot::disable()
{
    standby();
}

void Pilot::standby()
{
    logger.debug("[PILOT] Setting pilot HIGH (standby)");
    if(pwmAttached)
    {
        pwmAttached=false;
        // Stop PWM on this pin (toolchain provides ledcDetach)
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
    // Set pin as HIGH output
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);}

void Pilot::currentLimit(float amps)
{
    amps = constrain(amps, MIN_CURRENT, MAX_CURRENT);

            float dutyPercent;

    if (amps <= 51.0f)
        dutyPercent = amps / 0.6f;
    else
        dutyPercent = (amps / 2.5f) + 64.0f;

    dutyPercent = constrain(dutyPercent, 0.0f, 100.0f);

    currentDutyPercent = dutyPercent; // <-- store duty percent

    uint32_t maxDuty = (1UL << PILOT_PWM_RESOLUTION) - 1;
    uint32_t dutyCounts = (dutyPercent / 100.0f) * maxDuty;

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


float Pilot::readPin()
{
    int pinValue = analogReadMax(PIN_PILOT_IN, 100);
    float pinVoltage = (pinValue / 4095.0f) * 3.3f;

    this->voltage = pinVoltage * 5.44f - 12.6f;
    //logger.debugf("[PILOT] Analog: raw=%d, pinVoltage=%.2f, voltage=%.2f", pinValue, pinVoltage, this->voltage);
    return this->voltage;
}

float Pilot::getVoltage()
{
    logger.debugf("[PILOT] getVoltage -> %.2f V", this->voltage);
    return this->voltage;
}

VEHICLE_STATE_T Pilot::read()
{
    float voltage = floor(this->readPin());

    VEHICLE_STATE_T state;
    if (voltage >= 11) state = VEHICLE_NOT_CONNECTED;
    else if (voltage >= 8)  state = VEHICLE_CONNECTED;
    else if (voltage >= 5)  state = VEHICLE_READY;
    else if (voltage >= 2)  state = VEHICLE_READY_VENTILATION_REQUIRED;
    else if (voltage >= 0)  state = VEHICLE_NO_POWER;
    else state = VEHICLE_ERROR;
    
    // Log only on state change
    if (state != lastVehicleState) {
        lastVehicleState = state;
        char stateBuf[50];
        vehicleStateToText(state, stateBuf);
        logger.debugf("[PILOT] Read: voltage=%.0f -> %s", voltage, stateBuf);
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


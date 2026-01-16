#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <esp32-hal-ledc.h>
#include <limits.h>

#include "EvseLogger.h"
#include "Pilot.h"

/* =========================
 * PWM Configuration
 * ========================= */
constexpr int PIN_PILOT_PWM_OUT    = 27;
constexpr int PILOT_PWM_FREQ       = 1000;
constexpr int PILOT_PWM_RESOLUTION = 12;
constexpr int PILOT_PWM_MAX_DUTY   = (1 << PILOT_PWM_RESOLUTION) - 1;

/* =========================
 * Analog Sampling Configuration
 * ========================= */
constexpr int PIN_PILOT_IN = 36;
// Sample for 2 full PWM periods (2ms total) to ensure we hit the peaks
constexpr unsigned long SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ;

// Constructor - Clean and empty because variables are initialized in the header
Pilot::Pilot() 
{
}

void Pilot::begin()
{
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_PILOT_IN, ADC_11db);
    pinMode(PIN_PILOT_IN, INPUT);
    logger.info("[PILOT] ADC and PWM Pins configured");
}

void Pilot::standby()
{
    if(pwmAttached) {
        logger.debug("[PILOT] Detaching PWM for Standby (Static HIGH)");
        pwmAttached = false;
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);
}

void Pilot::disable()
{
    standby();
}

void Pilot::currentLimit(float amps)
{
    float dutyPercent = ampsToDuty(amps);
    currentDutyPercent = dutyPercent;

    uint32_t dutyCounts = (uint32_t)roundf((dutyPercent / 100.0f) * PILOT_PWM_MAX_DUTY);

    if(!pwmAttached) {
        ledcAttach(PIN_PILOT_PWM_OUT, PILOT_PWM_FREQ, PILOT_PWM_RESOLUTION);
        pwmAttached = true;
    }
    ledcWrite(PIN_PILOT_PWM_OUT, dutyCounts);
}

#include <limits.h> // Added for INT_MAX

VEHICLE_STATE_T Pilot::read()
{
    int highRaw = 0;
    int lowRaw = INT_MAX; // Use INT_MAX to ensure the first sample is always lower
    
    // 1. Timed sampling for peak detection
    unsigned long startTime = micros();
    while ((micros() - startTime) < SAMPLE_DURATION_US) {
        int val = analogReadMilliVolts(PIN_PILOT_IN);
        if (val > highRaw) highRaw = val;
        if (val < lowRaw)  lowRaw = val;
    }

    // 2. Convert to Pilot Levels
    highVoltageMv = (int)convertMv(highRaw);      // High peak (State Detection)
    lowVoltageMv = (int)convertMv(lowRaw);    // Low peak (Diode Detection)

    VEHICLE_STATE_T currentState;

    // 3. State Detection (High Peak) - Matches your second file's logic
    if (highVoltageMv >= VOLTAGE_STATE_NOT_CONNECTED) {
        currentState = VEHICLE_NOT_CONNECTED;
    } 
    else if (highVoltageMv >= VOLTAGE_STATE_CONNECTED) {
        currentState = VEHICLE_CONNECTED;
    } 
    else if (highVoltageMv >= VOLTAGE_STATE_READY) {
        currentState = VEHICLE_READY;
    } 
    else if (highVoltageMv >= VOLTAGE_STATE_VENTILATION) {
        currentState = VEHICLE_READY_VENTILATION_REQUIRED;
    } 
    else {
        // This covers the < 2V range (State E / Fault)
        currentState = VEHICLE_NO_POWER; 
    }

    // 4. Diode Check (The "Down Voltage" logic from your second file)
    // J1772 requires a negative swing to ~ -12V. 
    // If the PWM is ON (pwmAttached) but the low peak is too high (near 0V), 
    // it's a Diode Error (State F).
    if (pwmAttached && (currentState != VEHICLE_NOT_CONNECTED)) {
        if (lowVoltageMv > VOLTAGE_STATE_N12V_THRESHOLD) {
            // If low voltage is NOT negative enough, force an error
            currentState = VEHICLE_ERROR; 
        }
    }

    // 5. Logging on Change
    if (currentState != lastVehicleState) {
        lastVehicleState = currentState;
        char stateBuf[50];
        vehicleStateToText(currentState, stateBuf);
        logger.debugf("[PILOT] Change: %s (High: %d mV, Low: %d mV)", 
                      stateBuf, highVoltageMv, lowVoltageMv);
    }

    return currentState;
}

/* API & Helper Methods */
float Pilot::getVoltage() { return (float)highVoltageMv / 1000.0f; }
float Pilot::getPwmDuty() { return currentDutyPercent; }
float Pilot::convertMv(int adMv) { return ((float)adMv * PILOT_VOLTAGE_SCALE); }

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
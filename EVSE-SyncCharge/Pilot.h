/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the Pilot driver. Defines J1772 constants, PWM configuration,
 *              ADC settings, and the Pilot class interface for vehicle state detection.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "sdkconfig.h"
#include "EvseTypes.h" 

// =========================
// Constants - OFFICIAL SAE J1772 VALUES
// =========================

/* =========================================================================================
 * ADC to Pilot Voltage Calibration
 * =========================================================================================
 *
 * What is this?
 * -------------
 * The ESP32's Analog-to-Digital Converter (ADC) reads a voltage from the pilot
 * feedback circuit. However, this raw reading (in millivolts, or "mV") is not the
 * same as the actual J1772 pilot voltage (which swings from +12V to -12V).
 *
 * This section defines the constants needed to translate the ADC's measurement into
 * the real-world pilot voltage.
 *
 * How it works: 2-Point Linear Calibration
 * -----------------------------------------
 * We use a simple and effective method called 2-point calibration. It's like
 * drawing a straight line on a graph. We measure two known points, and then we can
 * find any other value along that line.
 *
 * Our two known points are the extremes of the pilot signal:
 *
 *   1. When the real pilot voltage is -12V, we measure that the ADC reads 350 mV.
 *      (CAL_ADC_MV_LOW = 350, CAL_PILOT_MV_LOW = -12000)
 *
 *   2. When the real pilot voltage is +12V, we measure that the ADC reads 3100 mV.
 *      (CAL_ADC_MV_HIGH = 3100, CAL_PILOT_MV_HIGH = 12000)
 *
 * Calculating the Slope
 * ---------------------
 * The 'slope' of this line tells us how many real millivolts the pilot signal
 * changes for every one millivolt change in the ADC reading.
 *
 *   Slope = (Change in Real Voltage) / (Change in ADC Reading)
 *         = (12000 - (-12000)) / (3100 - 350)
 *         = 24000 / 2752 = 8.72093
 *
 * This means for every 1 mV the ADC value goes up, the real pilot voltage has
 * gone up by ~8.72 mV.
 *
 * The Final Formula (in Pilot::convertMv)
 * ---------------------------------------
 * The code uses this formula to convert any new ADC reading:
 *   RealVoltage = StartVoltage + (ADC_Reading - Start_ADC_Reading) * Slope
 */
constexpr int CAL_ADC_MV_LOW     = 350;      // ADC reading at -12V
constexpr int CAL_PILOT_MV_LOW   = -12000;   // -12V in mV
constexpr int CAL_ADC_MV_HIGH    = 3100;     // ADC reading at +12V
constexpr int CAL_PILOT_MV_HIGH  = 12000;    // +12V in mV

// Slope = (12000 - (-12000)) / (3100 - 350) = 24000 / 2752 = 8.72093
constexpr float PILOT_CAL_SLOPE  = (float)(CAL_PILOT_MV_HIGH - CAL_PILOT_MV_LOW) / (float)(CAL_ADC_MV_HIGH - CAL_ADC_MV_LOW);


// Current limits
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 80.0f;

// J1772 PWM Conversion Constants
constexpr float J1772_LOW_RANGE_MAX_AMPS   = 51.0f;
constexpr float J1772_LOW_RANGE_MAX_DUTY   = 85.0f;
constexpr float J1772_LOW_RANGE_FACTOR     = 0.6f;
constexpr float J1772_HIGH_RANGE_FACTOR    = 2.5f;
constexpr float J1772_HIGH_RANGE_OFFSET    = 64.0f;

// Voltage thresholds (MilliVolts) based on SAE J1772 states after voltage divider
// State A: +12V (No vehicle) -> ~10.6V after divider
// State B: +9V  (Vehicle detected, not ready) -> ~8.0V after divider  
// State C: +6V  (Vehicle ready, charging) -> ~5.0V after divider
// State D: +3V  (Ventilation required) -> ~2.0V after divider
// State E/F: 0V/-12V (No power / Fault)
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 11000;  // J1772 State A threshold
constexpr int VOLTAGE_STATE_CONNECTED     =  8000;  // J1772 State B threshold
constexpr int VOLTAGE_STATE_READY         =  5000;  // J1772 State C threshold
constexpr int VOLTAGE_STATE_VENTILATION   =  2000;  // J1772 State D threshold
constexpr int VOLTAGE_STATE_N12V_THRESHOLD = 1000;  // Diode check: verify -12V swing present



/* =========================
 * PWM Configuration
 * ========================= */
#if CONFIG_IDF_TARGET_ESP32
constexpr int PIN_PILOT_PWM_OUT    = 27;
constexpr int PIN_PILOT_IN         = 36;
#elif CONFIG_IDF_TARGET_ESP32S3
constexpr int PIN_PILOT_PWM_OUT    = 14;
// GPIO 36 is NOT an ADC pin on ESP32-S3. We use GPIO 1 (ADC1_CH0) as default.
constexpr int PIN_PILOT_IN         = 1;
#endif

constexpr int PILOT_PWM_FREQ       = 1000;

constexpr int PILOT_PWM_RESOLUTION = 12;
constexpr int PILOT_PWM_MAX_DUTY   = (1 << PILOT_PWM_RESOLUTION) - 1;

/* =========================
 * Analog Sampling Configuration
 * ========================= */
// Sample for 2 full PWM periods (2ms total) to ensure we hit the peaks
constexpr unsigned long PILOT_SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ;

class Pilot {
private:    
    int highVoltageMv = 0; 
    int lowVoltageMv = 0;
    float currentDutyPercent = 0.0f;
    int rawHighMv = 0;
    int rawLowMv = 0;
    bool pwmAttached = false;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_NOT_CONNECTED; 

public:
    Pilot();
    ~Pilot();
    void begin();
    void standby();
    void disable();
    void stop();
    void currentLimit(float amps);
    VEHICLE_STATE_T read();
    float getVoltage();
    float getPwmDuty();
    float ampsToDuty(float amps);
    float dutyToAmps(float duty);
    int getRawHighMv() { return rawHighMv; }
    int getRawLowMv() { return rawLowMv; }

private:
    int analogReadMax();
    float convertMv(int adMv);
};

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif

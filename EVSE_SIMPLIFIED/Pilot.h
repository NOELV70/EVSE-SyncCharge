/*****************************************************************************
 * @file Pilot.h
 * @brief Pilot driver API and vehicle state helpers.
 ******************************************************************************/

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "EvseTypes.h" 

// =========================
// Constants - OFFICIAL SAE J1772 VALUES
// =========================

// Voltage divider scale factor (Matches your hardware)
constexpr float PILOT_VOLTAGE_SCALE = ((1200.0+3300.0)/1200.0)*1.2; 

// Current limits
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 80.0f;

// J1772 PWM Conversion Constants
constexpr float J1772_LOW_RANGE_MAX_AMPS   = 51.0f;
constexpr float J1772_LOW_RANGE_MAX_DUTY   = 85.0f;
constexpr float J1772_LOW_RANGE_FACTOR     = 0.6f;
constexpr float J1772_HIGH_RANGE_FACTOR    = 2.5f;
constexpr float J1772_HIGH_RANGE_OFFSET    = 64.0f;

// Voltage thresholds (MilliVolts) based on board_config style
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 11000; 
constexpr int VOLTAGE_STATE_CONNECTED     =  8000;
constexpr int VOLTAGE_STATE_READY         =  5000;
constexpr int VOLTAGE_STATE_VENTILATION   =  2000;
constexpr int VOLTAGE_STATE_N12V_THRESHOLD = 1000; // Threshold to verify -12V swing

// =========================
// Pilot class
// =========================
class Pilot {
private:    
    int highVoltageMv = 0;             // Stores the HIGH peak
    int lowVoltageMv = 0;          // Stores the LOW peak (for diode check)
    float currentDutyPercent = 0.0f;
    bool pwmAttached = false;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_ERROR; 

public:
    Pilot();
    void begin();
    void standby();
    void disable();
    
    /**
     * @brief Set pilot current limit in Amps
     */
    void currentLimit(float amps);
    
    /**
     * @brief Performs timed sampling to find High/Low peaks and returns state
     */
    VEHICLE_STATE_T read();

    // Getters
    float getVoltage();            // Returns high peak in Volts
    float getPwmDuty();            // Returns current duty cycle %
   
    // Conversion Helpers
    float ampsToDuty(float amps);
    float dutyToAmps(float duty);

private:
    int analogReadMax();           // Maintained for compatibility
    float convertMv(int adMv);     // Applies PILOT_VOLTAGE_SCALE
};

/**
 * @brief Converts VEHICLE_STATE_T to human readable string
 */
void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif
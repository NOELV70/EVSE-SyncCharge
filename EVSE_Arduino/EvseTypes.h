/*****************************************************************************
 * @file EvseTypes.h
 * @brief Common types and configuration defaults used across the EVSE project.
 *
 * @details
 * Defines enumerations for charging and vehicle states, the `ActualCurrent`
 * structure and `ChargingSettings` defaults.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef EVSE_TYPES_H_
#define EVSE_TYPES_H_

// Charging states
enum STATE_T {
    STATE_READY = 0,
    STATE_CHARGING = 1,
    STATE_COUNT
};


// Vehicle states
typedef enum VEHICLE_STATE {
    VEHICLE_NOT_CONNECTED,
    VEHICLE_CONNECTED,
    VEHICLE_READY,
    VEHICLE_READY_VENTILATION_REQUIRED,
    VEHICLE_NO_POWER,
    VEHICLE_ERROR,
    VEHICLE_STATE_COUNT
} VEHICLE_STATE_T;

// Actual measured currents
struct ActualCurrent {
    float l1 = 0.0f;
    float l2 = 0.0f;
    float l3 = 0.0f;
};

// Charging configuration
struct ChargingSettings {
    float maxCurrent = 32.0f;
    // If true, the AC relays will open when charging is paused.
    // If false, the pilot signal will be used to pause charging.
    bool acRelaisOpenAtPause = false;
    // If true, when the configured current limit falls below MIN_CURRENT
    // the pilot signal will be put into a pause/standby state. When the
    // current limit is raised again above MIN_CURRENT the pilot PWM will
    // resume automatically.
    // Enabled by default to pause pilot when current limit is below safe minimum.
    bool disableAtLowLimit = true;
    // Delay (milliseconds) to wait after pausing due to low current before
    // automatically resuming PWM when the current limit is raised above
    // MIN_CURRENT. Default is 5 minutes (300000 ms) to avoid rapid toggling.
    unsigned long lowLimitResumeDelayMs = 300000UL;
};


// Pause behavior mode for EVSE "pause" operations
// - PAUSE_STATE_A: Force CP to steady +12V (appears as State A if no EV; as B if EV connected); open relay immediately
// - PAUSE_STATE_B: Temporary pause (CP steady +12V); open relay with normal delay
typedef enum PauseMode {
    PAUSE_STATE_A = 0,
    PAUSE_STATE_B = 1
} PauseMode;

#endif

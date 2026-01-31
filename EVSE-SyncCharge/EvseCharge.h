/*****************************************************************************
 * @file EvseCharge.h
 * Header for the EVSE charging controller class.
 *
 * @details
 * Declares the `EvseCharge` class which handles charging state transitions,
 * current limit application, and integrates with `Pilot` and `Relay` modules.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef EVSE_CHARGE_H_
#define EVSE_CHARGE_H_

#include <Arduino.h>
#include "Pilot.h"
#include "Relay.h"
#include "EvseTypes.h"

typedef void (*EvseEventHandler)();

class EvseCharge {
public:
    EvseCharge(Pilot &pilotRef);
    void preinit_hard();
    void setup(ChargingSettings settings_);
    void loop();

    void startCharging();
    void stopCharging();
    void pauseCharging();

    STATE_T getState() const;
    VEHICLE_STATE_T getVehicleState() const;
    bool isVehicleConnected() const;
    bool isPaused() const;
    float getCurrentLimit() const;
    unsigned long getElapsedTime() const;

    void setCurrentLimit(float amps);
    // Configure behavior when current limit is below MIN_CURRENT (6A)
    // true = Allow continuous throttling (Solar mode); false = Strict J1772 (Pause/Stop)
    void setAllowBelow6AmpCharging(bool allow);
    bool getAllowBelow6AmpCharging() const;
    // Configure cooldown (ms) to wait after low-limit pause before auto-resume
    void setLowLimitResumeDelay(unsigned long ms);
    unsigned long getLowLimitResumeDelay() const;
    void updateActualCurrent(ActualCurrent current);
    ActualCurrent getActualCurrent() const;

    // RCM / RCD Control
    void setRcmEnabled(bool enable);
    bool isRcmEnabled() const;
    bool isRcmTripped() const;
    void setSafetyLockout(bool locked);
    bool isSafetyLockoutActive() const;

    float getPilotDuty() const;

    void enableCurrentTest(bool enable);
    void setCurrentTest(float amps);

    // ThrottleAlive (Safety Timeout)
    void setThrottleAliveTimeout(unsigned long seconds);
    void signalThrottleAlive();

    void onVehicleStateChange(EvseEventHandler handler);
    void onStateChange(EvseEventHandler handler);

private:
    void updateVehicleState();
    void applyCurrentLimit();
    void checkResumeFromLowLimit();
    void managePwmAndRelay();      // SAE J1772 state machine (PWM/relay automation)

private:
    Pilot* pilot;
    Relay* relay;

    STATE_T state = STATE_READY;
    VEHICLE_STATE_T vehicleState = VEHICLE_NOT_CONNECTED;
    ChargingSettings settings{};
    float currentLimit = 0.0f;
    unsigned long started = 0;

    ActualCurrent _actualCurrent{};
    unsigned long _actualCurrentUpdated = 0;

    bool currentTest = false;
    // When true the pilot was paused due to low current limit
    bool pausedAtLowLimit = false;
    bool userPaused = false;
    // Timestamp (millis) when pilot was paused due to low current limit
    unsigned long pausedSince = 0UL;
    // SAFETY: Error lockout defaults to TRUE (fail-safe) - prevents restart after crash/reboot
    // Only cleared when vehicle is safely disconnected (VEHICLE_NOT_CONNECTED state)
    bool errorLockout = true;
    bool rcmEnabled = true; // Default to enabled for safety
    bool rcmTripped = false; // Track specific RCM fault

    // ThrottleAlive State
    unsigned long throttleAliveTimeout = 0;
    unsigned long lastThrottleAliveTime = 0;
    unsigned long lastThrottleRampTime = 0;

    // RCM Periodic Test
    unsigned long lastRcmTestTime = 0;
    static const unsigned long RCM_TEST_INTERVAL = 86400000UL; // 24 Hours

    // Track previous vehicle state to detect error transitions
    VEHICLE_STATE_T lastManagedVehicleState = VEHICLE_NOT_CONNECTED;

    EvseEventHandler vehicleStateChange = nullptr;
    EvseEventHandler stateChange = nullptr;
};

#endif

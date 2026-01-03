/*****************************************************************************
 * @file EvseCharge.h
 * @brief Header for the EVSE charging controller class.
 *
 * @details
 * Declares the `EvseCharge` class which handles charging state transitions,
 * current limit application, and integrates with `Pilot` and `Relay` modules.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
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

    STATE_T getState() const;
    VEHICLE_STATE_T getVehicleState() const;
    float getCurrentLimit() const;
    unsigned long getElapsedTime() const;

    void setCurrentLimit(float amps);
    // Enable/disable automatic pilot pause when current limit below MIN_CURRENT
    void setDisableAtLowLimit(bool enable);
    bool getDisableAtLowLimit() const;
    // Configure cooldown (ms) to wait after low-limit pause before auto-resume
    void setLowLimitResumeDelay(unsigned long ms);
    unsigned long getLowLimitResumeDelay() const;
    void updateActualCurrent(ActualCurrent current);
    ActualCurrent getActualCurrent() const;

    float getPilotDuty() const;

    void enableCurrentTest(bool enable);
    void setCurrentTest(float amps);

    void onVehicleStateChange(EvseEventHandler handler);
    void onStateChange(EvseEventHandler handler);

private:
    void updateVehicleState();
    void applyCurrentLimit();
    void checkResumeFromLowLimit();

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
    // Timestamp (millis) when pilot was paused due to low current limit
    unsigned long pausedSince = 0UL;

    EvseEventHandler vehicleStateChange = nullptr;
    EvseEventHandler stateChange = nullptr;
};

#endif

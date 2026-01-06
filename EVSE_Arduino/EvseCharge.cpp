/*****************************************************************************
 * @file EvseCharge.cpp
 * @brief Implementation of the EVSE charging logic and state machine.
 *
 * @details
 * Implements the `EvseCharge` class which integrates `Pilot` and `Relay`
 * components to manage charging state, current limits, and safety behaviors.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#include "EvseCharge.h"
#include "EvseLogger.h"
#include <Arduino.h>

EvseCharge::EvseCharge(Pilot &pilotRef) {
    pilot = &pilotRef;
    relay = new Relay();
}

void EvseCharge::preinit_hard() {
    relay->setup(LOW);
}

void EvseCharge::setup(ChargingSettings settings_) {
    logger.info("[EVSE] Setup begin");
    relay->setup(LOW);
    pilot->standby();

    settings = settings_;
    currentLimit = settings.maxCurrent;
    vehicleState = VEHICLE_NOT_CONNECTED;
    state = STATE_READY;
    _actualCurrentUpdated = 0;
    
    // SAFETY: Initialize error lockout as FAIL-SAFE (true = locked)
    // Prevents restart immediately after watchdog reboot if vehicle was in error state
    // Only cleared after confirming vehicle is safely disconnected
    errorLockout = true;
    logger.info("[EVSE] Error lockout initialized (fail-safe)");

    logger.info("[EVSE] Setup done");
}

void EvseCharge::loop() {
    relay->loop();
    updateVehicleState();
    managePwmAndRelay();           // SAE J1772 state machine
    checkResumeFromLowLimit();
}

void EvseCharge::updateVehicleState() {
    VEHICLE_STATE_T newState = pilot->read();

    if (newState != vehicleState) {
        vehicleState = newState;

        char buf[50];
        vehicleStateToText(newState, buf);
        logger.infof("[EVSE] Vehicle state: %s", buf);

        if (vehicleState != VEHICLE_CONNECTED &&
            vehicleState != VEHICLE_READY &&
            vehicleState != VEHICLE_READY_VENTILATION_REQUIRED &&
            state == STATE_CHARGING) {
            stopCharging();
        }

        applyCurrentLimit();

        if (vehicleStateChange) vehicleStateChange();
    }
}

void EvseCharge::startCharging() {
    logger.info("[EVSE] startCharging() called");
    
    // SAFETY: Error lockout prevents restart after watchdog/crash recovery
    // Must be explicitly cleared when vehicle transitions to VEHICLE_NOT_CONNECTED
    if (errorLockout) {
        logger.warn("[EVSE] Start ignored: Error lockout ACTIVE - vehicle error/no-power detected (disconnect vehicle to clear)");
        return;
    }
    if (state == STATE_CHARGING) {
        logger.warn("[EVSE] Start ignored: Already charging");
        return;
    }
    if (vehicleState != VEHICLE_CONNECTED &&
        vehicleState != VEHICLE_READY &&
        vehicleState != VEHICLE_READY_VENTILATION_REQUIRED) {
        char stateBuf[32];
        vehicleStateToText(vehicleState, stateBuf);
        logger.warnf("[EVSE] Start ignored: Vehicle not ready (%s)", stateBuf);
        return;
    }
    logger.info("[EVSE] Start charging now");

    state = STATE_CHARGING;
    started = millis();
    applyCurrentLimit();
    if (stateChange) stateChange();
}

void EvseCharge::stopCharging() {
    logger.info("[EVSE] stopCharging() called");
    relay->openImmediately();
    if (state != STATE_CHARGING) {
        logger.warn("[EVSE] Stop ignored: Not charging");
        return;
    }

    logger.info("[EVSE] Stop charging");
    state = STATE_READY;
    if (stateChange) stateChange();
}

STATE_T EvseCharge::getState() const {
//    logger.debugf("[EVSE] getState -> %d", (int)state);
    return state;
}

VEHICLE_STATE_T EvseCharge::getVehicleState() const {
//    char buf[50];
//    vehicleStateToText(vehicleState, buf);
//    logger.debugf("[EVSE] getVehicleState -> %s", buf);
    return vehicleState;
}

float EvseCharge::getCurrentLimit() const {
//    logger.debugf("[EVSE] getCurrentLimit -> %.2f A", currentLimit);
    return currentLimit;
}

unsigned long EvseCharge::getElapsedTime() const {
    unsigned long e = millis() - started;
//    logger.debugf("[EVSE] getElapsedTime -> %lu ms", e);
    return e;
}

void EvseCharge::setCurrentLimit(float amps) {
    if (amps < 0) amps = 0;
    if (amps > settings.maxCurrent) amps = settings.maxCurrent;

    if (amps != currentLimit) {
        currentLimit = amps;
        logger.infof("[EVSE] Setting current limit to %.2f A", amps);
        applyCurrentLimit();
    }
}

void EvseCharge::updateActualCurrent(ActualCurrent current) {
    _actualCurrent = current;
    _actualCurrentUpdated = millis();

    logger.infof("[EVSE] Actual current L1,L2,L3: %.2f %.2f %.2f", current.l1, current.l2, current.l3);
}

ActualCurrent EvseCharge::getActualCurrent() const {
//    logger.debugf("[EVSE] getActualCurrent -> L1: %.2f, L2: %.2f, L3: %.2f",
//                 _actualCurrent.l1, _actualCurrent.l2, _actualCurrent.l3);
    return _actualCurrent;
}

float EvseCharge::getPilotDuty() const {
    float duty = pilot->getPwmDuty();
 //   logger.debugf("[EVSE] getPilotDuty -> %.2f", duty);
    return duty;
}

void EvseCharge::enableCurrentTest(bool enable) {
    if (enable && state == STATE_CHARGING) {
        logger.warn("[EVSE] Test rejected: charging active");
        return;
    }
    currentTest = enable;
    logger.info(enable ? "[EVSE] Test mode ENABLED" : "[EVSE] Test mode DISABLED");
    pilot->standby();
}

void EvseCharge::setCurrentTest(float amps) {
    if (!currentTest) return;
    if (amps < MIN_CURRENT) amps = MIN_CURRENT;
    if (amps > settings.maxCurrent) amps = settings.maxCurrent;

    logger.infof("[EVSE] Test current set to %.2f A", amps);
    pilot->currentLimit(amps);
}

void EvseCharge::onVehicleStateChange(EvseEventHandler handler) { vehicleStateChange = handler; }
void EvseCharge::onStateChange(EvseEventHandler handler) { stateChange = handler; }

void EvseCharge::checkResumeFromLowLimit() {
    // If we are paused and the current is now high enough, check if the delay has passed.
    if (pausedAtLowLimit && currentLimit >= MIN_CURRENT) {
        unsigned long now = millis();
        unsigned long elapsed = (now >= pausedSince) ? (now - pausedSince) : 0UL;

        if (elapsed >= settings.lowLimitResumeDelayMs) {
            logger.info("[EVSE] Low-limit pause delay elapsed. Resuming.");
            applyCurrentLimit(); // This will handle the actual resume.
        }
    }
}

void EvseCharge::applyCurrentLimit() {
    if (vehicleState == VEHICLE_CONNECTED ||
        vehicleState == VEHICLE_READY ||
        vehicleState == VEHICLE_READY_VENTILATION_REQUIRED) {

        if (currentLimit >= MIN_CURRENT) {
            // If we previously paused due to low-limit, only resume after the
            // configured cooldown in settings.lowLimitResumeDelayMs has elapsed.
            if (pausedAtLowLimit) {
                unsigned long now = millis();
                unsigned long elapsed = (now >= pausedSince) ? (now - pausedSince) : 0UL;
                if (elapsed >= settings.lowLimitResumeDelayMs) {
                    // Resume PWM with current limit (this attaches PWM if needed)
                    pilot->currentLimit(currentLimit);
                    logger.info("[EVSE] Resuming pilot PWM after low-limit pause");
                    pausedAtLowLimit = false;
                } else {
                    // Still in cooldown period: keep pilot in standby and relay open
                    // to avoid rapid toggling. Do not change relay/state here.
                    return;
                }
            } else {
                // Normal resume/apply
                pilot->currentLimit(currentLimit);
            }

            if (state == STATE_CHARGING &&
                (vehicleState == VEHICLE_READY || vehicleState == VEHICLE_READY_VENTILATION_REQUIRED))
                relay->close();
            else
                relay->open();
        } else {
            // Current limit below minimum (dynamic power throttling for solar budget)
            if (settings.disableAtLowLimit) {
                // PAUSE MODE: Maintain PWM with reduced duty instead of hard standby
                // Vehicle interprets continuous low-duty PWM as reduced charging capacity
                // Relay controlled per configuration; resume after delay
                pilot->currentLimit(currentLimit);  // Keep PWM, just lower duty
                
                if (settings.acRelaisOpenAtPause) {
                    relay->openImmediately();
                } else {
                    relay->open();
                }
                if (!pausedAtLowLimit) {
                    logger.infof("[EVSE] Low power pause: PWM set to %.2f A (solar budget insufficient)", currentLimit);
                    pausedAtLowLimit = true;
                    pausedSince = millis();
                }
            } else {
                // THROTTLE MODE: Allow current below MIN_CURRENT for continuous solar throttling
                // No pause/resume delay logic - direct PWM adjustment
                logger.infof("[EVSE] Applying low current limit: %.2f A (solar throttling)", currentLimit);
                pilot->currentLimit(currentLimit);
                // Clear pause flag since we're not actually pausing, just throttling
                pausedAtLowLimit = false;
            }
        }
    } else {
        relay->open();
        pilot->standby();
        // If car is unplugged, we are no longer "paused at low limit"
        if (pausedAtLowLimit) {
            pausedAtLowLimit = false;
        }
    }
}

void EvseCharge::setDisableAtLowLimit(bool enable) {
    settings.disableAtLowLimit = enable;
    logger.infof("[EVSE] disableAtLowLimit set to %s", enable ? "ENABLED" : "DISABLED");
    // Immediately apply behavior in case currentLimit is below threshold
    applyCurrentLimit();
}

bool EvseCharge::getDisableAtLowLimit() const {
//    logger.debugf("[EVSE] getDisableAtLowLimit -> %s", settings.disableAtLowLimit ? "ENABLED" : "DISABLED");
    return settings.disableAtLowLimit;
}

void EvseCharge::setLowLimitResumeDelay(unsigned long ms) {
    settings.lowLimitResumeDelayMs = ms;
    logger.infof("[EVSE] lowLimitResumeDelayMs set to %lu ms", ms);
}

/* =========================
 * SAE J1772 State Machine
 * ========================= */

void EvseCharge::managePwmAndRelay() {
    // Detect vehicle state transitions for error handling
    if (vehicleState != lastManagedVehicleState) {
        lastManagedVehicleState = vehicleState;
        
        // Handle error and no-power states
        if (vehicleState == VEHICLE_ERROR || vehicleState == VEHICLE_NO_POWER) {
            if (!errorLockout) {
                errorLockout = true;
                char stateBuf[32];
                vehicleStateToText(vehicleState, stateBuf);
                logger.warnf("[EVSE] Error lockout activated: %s", stateBuf);
                if (state == STATE_CHARGING) {
                    stopCharging();  // Emergency stop
                }
            }
        }
        // SAFETY: Clear error lockout only when vehicle is safely disconnected (fail-safe recovery)
        // This is the only safe path to recover from error/no-power states
        else if (vehicleState == VEHICLE_NOT_CONNECTED && errorLockout) {
            errorLockout = false;
            logger.warn("[EVSE] Error lockout CLEARED: Vehicle fully disconnected (safe to accept new start commands)");
        }
    }
    
    // State machine: Manage PWM and relay based on vehicle state and charging state
    // 
    // SAE J1772 Spec Firmware Actions:
    // State A (NOT_CONNECTED):      PWM Off; Relay Open
    // State B (CONNECTED):          PWM On (low); Relay Open (vehicle in standby)
    // State C (READY):              PWM On (full); Relay Closed (if charging)
    // State D (VENTILATION):        PWM On (full); Relay Closed (if charging, log vent)
    // State E/F (ERROR):            Emergency Stop; Lockout
    // State 4 (NO_POWER):           PWM Off; Relay Open; Lockout
    
    switch (vehicleState) {
        case VEHICLE_NOT_CONNECTED:
            // State A: No vehicle detected
            pilot->standby();
            relay->open();
            break;
            
        case VEHICLE_CONNECTED:
            // State B: Vehicle detected but not ready
            // Keep PWM on at low level to signal readiness, relay open
            // User can start charging from here (but must be current-limited)
            if (state != STATE_CHARGING) {
                pilot->currentLimit(MIN_CURRENT);  // Low current signaling
            }
            relay->open();
            break;
            
        case VEHICLE_READY:
            // State C: Vehicle ready for charging
            if (state == STATE_CHARGING) {
                // Apply current limit and close relay
                pilot->currentLimit(currentLimit);
                relay->close();
            } else {
                // Not charging but ready: maintain low current signal
                pilot->currentLimit(MIN_CURRENT);
                relay->open();
            }
            break;
            
        case VEHICLE_READY_VENTILATION_REQUIRED:
            // State D: Vehicle ready with ventilation requirement
            logger.info("[EVSE] Vehicle ventilation mode detected");
            if (state == STATE_CHARGING) {
                // Apply current limit and close relay
                pilot->currentLimit(currentLimit);
                relay->close();
            } else {
                // Not charging but ready: maintain low current signal
                pilot->currentLimit(MIN_CURRENT);
                relay->open();
            }
            break;
            
        case VEHICLE_NO_POWER:
            // State 4: No power detected - error condition
            pilot->standby();
            relay->open();
            break;
            
        case VEHICLE_ERROR:
            // State E/F: Error condition - emergency stop
            pilot->standby();
            relay->openImmediately();
            break;
            
        default:
            pilot->standby();
            relay->open();
            break;
    }
}
unsigned long EvseCharge::getLowLimitResumeDelay() const {
//    logger.debugf("[EVSE] getLowLimitResumeDelay -> %lu ms", settings.lowLimitResumeDelayMs);
    return settings.lowLimitResumeDelayMs;
}

/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the core EVSE charging logic. Manages the J1772 state
 *              machine, relay control, safety timers, and current limiting.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "EvseCharge.h"
#include "Rcm.h"
#include "EvseLogger.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

extern Rcm rcm;

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
    pilot->begin();
    pilot->standby();

    settings = settings_;
    currentLimit = settings.maxCurrent;
    vehicleState = VEHICLE_NOT_CONNECTED;
    state = STATE_READY;
    _actualCurrentUpdated = 0;
    userPaused = false;
    
    // SAFETY: Initialize error lockout as FAIL-SAFE (true = locked)
    // Prevents restart immediately after watchdog reboot if vehicle was in error state
    // Only cleared after confirming vehicle is safely disconnected
    errorLockout = true;
    logger.info("[EVSE] Error lockout initialized (fail-safe)");
    lastRcmTestTime = millis(); // Initialize timer (power-on test is handled in main setup)

    logger.info("[EVSE] Setup done");
}

void EvseCharge::loop() {
    // Safety: Check Residual Current Monitor
    if (rcmEnabled && rcm.isTriggered()) {
        logger.error("[EVSE] CRITICAL: RCM Fault Detected! Emergency Stop.");
        relay->openImmediately();
        stopCharging();
        rcmTripped = true;
        if (!errorLockout) {
            errorLockout = true;
            logger.warn("[EVSE] Error lockout activated due to RCM Fault");
        }
    }

    // Periodic RCM Self-Test (IEC 62955 / IEC 61851 recommendation: every 24h)
    // Only run if not charging to avoid interruption.
    if (rcmEnabled && state != STATE_CHARGING && (millis() - lastRcmTestTime > RCM_TEST_INTERVAL)) {
        logger.info("[EVSE] Performing periodic 24h RCM self-test...");
        if (rcm.selfTest()) {
            lastRcmTestTime = millis();
            logger.info("[EVSE] Periodic RCM test PASSED");
        } else {
            logger.error("[EVSE] Periodic RCM test FAILED! Entering Lockout.");
            rcmTripped = true;
            errorLockout = true;
            relay->openImmediately();
        }
    }

    relay->loop();
    updateVehicleState();
    managePwmAndRelay();           // SAE J1772 state machine
    checkResumeFromLowLimit();

    // ThrottleAlive Logic (Centralized Safety)
    // If enabled (>0) and charging, check if external control data is stale.
    if (throttleAliveTimeout > 0 && state == STATE_CHARGING) {
        unsigned long now = millis();
        if ((now - lastThrottleAliveTime) > (throttleAliveTimeout * 1000UL)) {
            // Data is stale. Check if we need to ramp down.
            if (currentLimit > 6.0f) {
                // Ramp down by 1A every 5 seconds
                if (now - lastThrottleRampTime >= 5000UL) {
                    float next = currentLimit - 1.0f;
                    if (next < 6.0f) next = 6.0f;
                    logger.warnf("[EVSE] ThrottleAlive: Stale data. Ramping %.1fA -> %.1fA", currentLimit, next);
                    setCurrentLimit(next);
                    lastThrottleRampTime = now;
                }
            }
        } else {
            // Data is fresh. Reset ramp timer so first drop happens immediately on timeout.
            lastThrottleRampTime = now - 5000UL; 
        }
    }
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

    // SAFETY: Pre-charge RCM Self-Test (IEC 61851 / IEC 62955)
    // Must verify RCM is functional before closing contactor.
    if (rcmEnabled) {
        logger.info("[EVSE] Pre-charge RCM self-test initiating...");
        if (!rcm.selfTest()) {
            logger.error("[EVSE] Pre-charge RCM test FAILED. Aborting charge.");
            rcmTripped = true;
            errorLockout = true;
            relay->openImmediately();
            return;
        }
        logger.info("[EVSE] Pre-charge RCM test PASSED.");
        // Update periodic timer so we don't re-test unnecessarily soon
        lastRcmTestTime = millis();
    }

    logger.info("[EVSE] Start charging now");

    state = STATE_CHARGING;
    started = millis();
    userPaused = false; // Clear pause flag on start/resume
    lastThrottleAliveTime = millis(); // Reset ThrottleAlive timer on start
    applyCurrentLimit();
    if (stateChange) stateChange();
}

void EvseCharge::stopCharging() {
    logger.info("[EVSE] stopCharging() called");
    relay->openImmediately();
    if (state != STATE_CHARGING) {
        userPaused = false; // Ensure pause flag is cleared if we force stop from non-charging state
        logger.warn("[EVSE] Stop ignored: Not charging");
        return;
    }

    logger.info("[EVSE] Stop charging");
    state = STATE_READY;
    userPaused = false; // Clear pause flag on explicit stop
    if (stateChange) stateChange();
}

void EvseCharge::pauseCharging() {
    if (state == STATE_CHARGING) {
        logger.info("[EVSE] pauseCharging() called");
        // We call stopCharging logic manually here to avoid clearing the userPaused flag
        // which stopCharging() normally does.
        relay->openImmediately();
        state = STATE_READY;
        userPaused = true;
        if (stateChange) stateChange();
    } else {
        logger.warn("[EVSE] Pause ignored: Not charging");
    }
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

bool EvseCharge::isVehicleConnected() const {
    return (vehicleState == VEHICLE_CONNECTED || 
            vehicleState == VEHICLE_READY || 
            vehicleState == VEHICLE_READY_VENTILATION_REQUIRED);
}

bool EvseCharge::isPaused() const {
    return userPaused;
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

    logger.infof("[EVSE] Test current set to %.2f A", amps);
    pilot->currentLimit(amps);
}

void EvseCharge::setThrottleAliveTimeout(unsigned long seconds) {
    throttleAliveTimeout = seconds;
    logger.infof("[EVSE] ThrottleAlive timeout set to %lu s", seconds);
}

void EvseCharge::signalThrottleAlive() {
    lastThrottleAliveTime = millis();
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
    // TEST MODE: Keep PWM running (ignore vehicle state) and force Relay OPEN
    if (currentTest) {
        relay->open();
        return;
    }

    // J1772 Compliance: If not charging (and not in test mode), force DC Standby.
    // Prevents sending PWM (State B2) when we are only in State B1 (EVSE Ready, but not authorized).
    if (state != STATE_CHARGING) {
        pilot->standby();
        relay->open();
        return;
    }

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

void EvseCharge::setAllowBelow6AmpCharging(bool allow) {
    settings.disableAtLowLimit = !allow; // Inverted logic: Allow=true means DisablePause=true (wait, DisablePause=false) -> DisableAtLowLimit=false
    logger.infof("[EVSE] AllowBelow6AmpCharging set to %s", allow ? "TRUE (Throttle)" : "FALSE (Strict J1772)");
    // Immediately apply behavior in case currentLimit is below threshold
    applyCurrentLimit();
}

bool EvseCharge::getAllowBelow6AmpCharging() const {
//    logger.debugf("[EVSE] getAllowBelow6AmpCharging -> %s", !settings.disableAtLowLimit ? "TRUE" : "FALSE");
    return !settings.disableAtLowLimit;
}

void EvseCharge::setLowLimitResumeDelay(unsigned long ms) {
    settings.lowLimitResumeDelayMs = ms;
    logger.infof("[EVSE] lowLimitResumeDelayMs set to %lu ms", ms);
}

/* =========================
 * SAE J1772 State Machine
 * ========================= */

void EvseCharge::managePwmAndRelay() {
    // TEST MODE: Skip J1772 state enforcement to keep PWM running
    if (currentTest) {
        relay->open();
        return;
    }

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
            rcmTripped = false; // Reset RCM trip flag when vehicle is unplugged
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
            // J1772: If not charging, offer DC (Standby), not PWM.
            // PWM implies "Power Available" which might confuse the car if we aren't started.
            if (state != STATE_CHARGING) pilot->standby();
            relay->open();
            break;
            
        case VEHICLE_READY:
            // State C: Vehicle ready for charging
            if (state == STATE_CHARGING) {
                // Apply current limit and close relay
                pilot->currentLimit(currentLimit);
                relay->close();
            } else {
                // Not charging: Force DC Standby. Tells car "Wait".
                pilot->standby();
                relay->open();
            }
            break;
            
        case VEHICLE_READY_VENTILATION_REQUIRED:
            // State D: Vehicle ready with ventilation requirement
            //logger.info("[EVSE] Vehicle ventilation mode detected");
            if (state == STATE_CHARGING) {
                // Apply current limit and close relay
                pilot->currentLimit(currentLimit);
                relay->close();
            } else {
                // Not charging: Force DC Standby.
                pilot->standby();
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

void EvseCharge::setRcmEnabled(bool enable) {
    rcmEnabled = enable;
    logger.infof("[EVSE] RCM Safety Check %s", enable ? "ENABLED" : "DISABLED");
}

bool EvseCharge::isRcmEnabled() const {
    return rcmEnabled;
}

bool EvseCharge::isRcmTripped() const {
    return rcmTripped;
}

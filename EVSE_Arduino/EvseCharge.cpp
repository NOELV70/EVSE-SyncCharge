/*****************************************************************************
 * @file EvseCharge.cpp
 * @brief Implementation of the EVSE charging logic and state machine.
 *
 * @details
 * Implements the `EvseCharge` class which integrates `Pilot` and `Relay`
 * components to manage charging state, current limits, and safety behaviors.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
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

    logger.info("[EVSE] Setup done");
}

void EvseCharge::loop() {
    relay->loop();
    updateVehicleState();
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
    if (state == STATE_CHARGING) {
        logger.warn("[EVSE] Start ignored: Already charging");
        return;
    }
    if (vehicleState != VEHICLE_CONNECTED &&
        vehicleState != VEHICLE_READY &&
        vehicleState != VEHICLE_READY_VENTILATION_REQUIRED) {
        logger.warnf("[EVSE] Start ignored: Vehicle not ready (State: %d)", vehicleState);
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
            // Current limit below minimum
            if (settings.disableAtLowLimit) {
                // Put pilot into pause/standby (no PWM). If vehicle is connected, CP will be interpreted as State B (~9V via vehicle pull-down),
                // otherwise State A (~12V). Control relay behavior per configuration to emulate A- or B-like pause.
                pilot->standby();
                if (settings.acRelaisOpenAtPause) {
                    // A-like hard pause: open immediately
                    relay->openImmediately();
                } else {
                    // B-like temporary pause: open with safety delay
                    relay->open();
                }
                if (!pausedAtLowLimit) {
                    logger.info("[EVSE] Paused pilot due to low current limit (< MIN_CURRENT)");
                    pausedAtLowLimit = true;
                    pausedSince = millis();
                }
            } else {
                logger.info("[EVSE] Pilot will be limited to low current limit ( == MIN_CURRENT)");
                // Normal apply - minit to min in PILOT code ! 
                pilot->currentLimit(currentLimit);
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

unsigned long EvseCharge::getLowLimitResumeDelay() const {
//    logger.debugf("[EVSE] getLowLimitResumeDelay -> %lu ms", settings.lowLimitResumeDelayMs);
    return settings.lowLimitResumeDelayMs;
}

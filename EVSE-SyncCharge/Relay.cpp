/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the Relay driver. Provides non-blocking control of the
 *              main contactor with anti-chatter hysteresis and safety delays.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "Relay.h"
#include "EvseLogger.h"

/* =========================
 * Hardware constants
 * ========================= */
constexpr int PIN_RELAY_OUT = 16; // Digital output to control relay coil
#define RELAY_SWITCH_DELAY  3000UL


Relay::Relay()
        : _currentState(false),
          _desiredState(false),
          _lastSwitchTime(0UL)
{
}

void Relay::setup(bool initialState)
{
    _currentState = initialState;
    _desiredState = initialState;

    pinMode(PIN_RELAY_OUT, OUTPUT);
    digitalWrite(PIN_RELAY_OUT, initialState);
    logger.infof("[RELAY] Initialized: %s", initialState ? "CLOSED" : "OPEN");
}

void Relay::loop()
{
    if (_desiredState != _currentState)
    {
        // Anti-chatter: only switch if enough time has passed since the last physical switch.
        // The check for _lastSwitchTime == 0 allows the very first switch to be immediate.
        // We also allow immediate OPEN (LOW) for safety and responsiveness.
        if (_desiredState == LOW || _lastSwitchTime == 0 || (millis() - _lastSwitchTime) >= RELAY_SWITCH_DELAY)
        {
            _currentState = _desiredState;
            digitalWrite(PIN_RELAY_OUT, _currentState);
            logger.infof("[RELAY] Switched to %s", _currentState ? "CLOSED" : "OPEN");
            _lastSwitchTime = millis(); // Record the time of this switch
        }
    }
}

void Relay::open()
{
    if (_desiredState != LOW) {
        _desiredState = LOW;
        logger.debug("[RELAY] Open requested");
    }
}

void Relay::close()
{
    if (_desiredState != HIGH) {
        _desiredState = HIGH;
        logger.debug("[RELAY] Close requested");
    }
}

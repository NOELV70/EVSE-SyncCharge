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

/* =========================
 * Hardware constants
 * ========================= */
constexpr int PIN_RELAY_OUT = 16; // Digital output to control relay coil
#define RELAY_SWITCH_DELAY  3000UL


Relay::Relay()
        : _currentState(false),
            _desiredState(false),
            _lastCalledMillis(0UL)
{
}

void Relay::setup(bool initialState)
{
    _currentState = initialState;
    _desiredState = initialState;

    pinMode(PIN_RELAY_OUT, OUTPUT);
    digitalWrite(PIN_RELAY_OUT, initialState);
}

void Relay::loop()
{
    if (_desiredState != _currentState)
    {
        if ((millis() - _lastCalledMillis) >= RELAY_SWITCH_DELAY)
        {
            _currentState = _desiredState;
            digitalWrite(PIN_RELAY_OUT, _currentState);
        }
    }
}

void Relay::openImmediately()
{
    _desiredState = LOW;
    _currentState = LOW;
    digitalWrite(PIN_RELAY_OUT, LOW);
}

void Relay::open()
{
    _desiredState = LOW;
    _lastCalledMillis = millis();
}

void Relay::close()
{
    _desiredState = HIGH;
    _lastCalledMillis = millis();
}

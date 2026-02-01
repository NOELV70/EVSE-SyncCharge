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
constexpr int PIN_RELAY_L1 = 14;    // RELAISL1: Main Contactor (L1 + N) [ADC2_CH6]
constexpr int PIN_RELAY_L2L3 = 13;  // RELAISL2L3: Phase Expansion (L2 + L3) [ADC2_CH4]

constexpr unsigned long RELAY_SWITCH_DELAY  = 3000UL;
constexpr unsigned long PHASE_SWITCH_SAFETY_DELAY_MS = 15000UL; // 15s Capacitor Discharge Delay

Relay::Relay()
        : _currentState(false),
          _desiredState(false),
          _lastSwitchTime(0UL),
          _isThreePhaseMode(false),
          _safetyLockoutActive(false),
          _safetyLockoutStart(0)
{
}

void Relay::setup()
{
    _currentState = false;
    _desiredState = false;

    pinMode(PIN_RELAY_L1, OUTPUT);
    digitalWrite(PIN_RELAY_L1, LOW);

    pinMode(PIN_RELAY_L2L3, OUTPUT);
    // Safety: Always init L2/L3 to OFF until mode is confirmed
    digitalWrite(PIN_RELAY_L2L3, LOW);

    logger.info("[RELAY] Initialized: OPEN");
}

void Relay::loop()
{
    // 1. Handle Phase Switch Safety Lockout
    // If we recently switched phases, we MUST keep relays open to allow capacitors to discharge.
    if (_safetyLockoutActive) {
        if (millis() - _safetyLockoutStart >= PHASE_SWITCH_SAFETY_DELAY_MS) {
            _safetyLockoutActive = false;
            logger.info("[RELAY] Phase Switch Safety Delay Ended. Ready.");
            // Reset switch timer to allow immediate re-close if desired
            _lastSwitchTime = 0; 
        } else {
            // FORCE OPEN during lockout
            if (_currentState) {
                digitalWrite(PIN_RELAY_L1, LOW);
                digitalWrite(PIN_RELAY_L2L3, LOW);
                _currentState = false;
            }
            return; // Block normal logic
        }
    }

    // 2. Normal Anti-Chatter Logic
    if (_desiredState != _currentState)
    {
        // Anti-chatter: only switch if enough time has passed since the last physical switch.
        // The check for _lastSwitchTime == 0 allows the very first switch to be immediate.
        // We also allow immediate OPEN (LOW) for safety and responsiveness.
        if (_desiredState == LOW || _lastSwitchTime == 0 || (millis() - _lastSwitchTime) >= RELAY_SWITCH_DELAY)
        {
            _currentState = _desiredState;
            
            // RELAISL1: Always follows the state
            digitalWrite(PIN_RELAY_L1, _currentState);

            // RELAISL2L3: Only closes if Main is Closed AND we are in 3-Phase Mode
            // If Opening, we turn OFF everything.
            bool l2l3_state = _currentState && _isThreePhaseMode;
            digitalWrite(PIN_RELAY_L2L3, l2l3_state);

            logger.infof("[RELAY] Switched to %s (%s)", 
                _currentState ? "CLOSED" : "OPEN",
                _currentState ? (_isThreePhaseMode ? "3-Phase" : "1-Phase") : "OFF");
                
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

void Relay::setThreePhase(bool enable)
{
    if (_isThreePhaseMode != enable) {
        logger.infof("[RELAY] Phase Mode Change: %s -> %s", 
            _isThreePhaseMode ? "3-Phase" : "1-Phase", 
            enable ? "3-Phase" : "1-Phase");

        _isThreePhaseMode = enable;
        
        // SAFETY: Force immediate disconnect of ALL phases
        digitalWrite(PIN_RELAY_L1, LOW);
        digitalWrite(PIN_RELAY_L2L3, LOW);
        _currentState = false;
        _desiredState = LOW; // Safety: Ensure logic state matches physical state
        
        // Engage Safety Lockout to allow capacitor discharge
        _safetyLockoutActive = true;
        _safetyLockoutStart = millis();
        logger.warn("[RELAY] Safety Lockout Active (Capacitor Discharge)");
    }
}

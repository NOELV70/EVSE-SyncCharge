/*****************************************************************************
 * @file Relay.h
 * Small helper to control the hardware relay with safety timing.
 *
 * @details
 * Declares `Relay` which provides delayed switching and immediate open
 * behavior required by the EVSE safety logic.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef RELAY_H_
#define RELAY_H_

#include <Arduino.h>

class Relay
{
private:
    bool _currentState;
    bool _desiredState;
    unsigned long _lastSwitchTime;

public:
    Relay();

    void setup(bool initialState);
    void loop();

    void open();
    void close();
    
    // Status getters for safety sequencing
    bool isClosed() const { return _currentState == HIGH; }
    bool isOpen() const { return _currentState == LOW; }
    bool isPending() const { return _desiredState != _currentState; }
};

#endif // RELAY_H_

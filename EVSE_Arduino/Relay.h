/*****************************************************************************
 * @file Relay.h
 * @brief Small helper to control the hardware relay with safety timing.
 *
 * @details
 * Declares `Relay` which provides delayed switching and immediate open
 * behavior required by the EVSE safety logic.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
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
    unsigned long _lastCalledMillis;

public:
    Relay();

    void setup(bool initialState);
    void loop();

    void openImmediately();
    void open();
    void close();
};

#endif // RELAY_H_

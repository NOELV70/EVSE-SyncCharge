#ifndef RCM_H
#define RCM_H

#include <Arduino.h>

class Rcm {
public:
    Rcm();
    void begin();
    bool selfTest();
    bool isTriggered();
};

#endif
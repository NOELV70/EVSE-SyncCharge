/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the RGBWL2812 LED driver. Defines LED state enums,
 *              color definitions, and configuration structures for visual effects.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef RGBWL2812_H
#define RGBWL2812_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// LED Effects
enum LedEffect {
    EFF_OFF = 0,
    EFF_SOLID,
    EFF_BLINK_SLOW,
    EFF_BLINK_FAST,
    EFF_BREATH,
    EFF_RAINBOW,
    EFF_KNIGHT_RIDER,
    EFF_CHASE
};

// LED Colors
enum LedColor {
    COL_OFF = 0,
    COL_RED,
    COL_GREEN,
    COL_BLUE,
    COL_YELLOW,
    COL_CYAN,
    COL_MAGENTA,
    COL_WHITE
};

struct LedStateSetting {
    LedColor color;
    LedEffect effect;
};

struct LedSettings {
    bool enabled;
    uint16_t numLeds;
    LedStateSetting stateStandby;   // A
    LedStateSetting stateConnected; // B
    LedStateSetting stateCharging;  // C
    LedStateSetting stateError;     // F
    LedStateSetting stateWifi;      // AP Mode
    LedStateSetting stateBoot;      // Boot
    LedStateSetting stateSolarIdle; // Solar Throttling (<6A)
};

// EVSE Status States for the LED
enum EvseLedState {
    LED_OFF_STATE,
    LED_BOOT,           // Rainbow cycle
    LED_READY,          // Steady Green
    LED_CONNECTED,      // Steady Yellow/Orange
    LED_CHARGING,       // Breathing Cyan/Blue
    LED_ERROR,          // Fast Blinking Red
    LED_WIFI_CONFIG,    // Slow Blinking Blue
    LED_SOLAR_IDLE      // Solar Idle / Low Power
};

class RGBWL2812 {
public:
    // Constructor: Configure pin
    RGBWL2812(int pin);

    void begin();
    void loop(); // Call this in the main loop for animations
    
    void setState(EvseLedState state);
    void startTestSequence();
    
    // Config
    void loadConfig();
    void saveConfig();
    LedSettings& getConfig() { return _config; }
    void updateConfig(LedSettings newConfig);

private:
    Adafruit_NeoPixel _strip;
    int _pin;
    LedSettings _config;
    EvseLedState _currentState;
    
    unsigned long _lastUpdate;
    int _animStep;
    bool _animDir;
    bool _testMode;
    unsigned long _testSequenceStart;
    int _currentTestStep;

    void setAll(uint32_t color);
    uint32_t getColor(LedColor c);
    void runEffect(LedStateSetting setting);
    uint32_t Wheel(byte WheelPos);
};

extern RGBWL2812 led;

#endif
#ifndef RGBWL2812_H
#define RGBWL2812_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// EVSE Status States for the LED
enum EvseLedState {
    LED_OFF,
    LED_BOOT,           // Rainbow cycle
    LED_READY,          // Steady Green
    LED_CONNECTED,      // Steady Yellow/Orange
    LED_CHARGING,       // Breathing Cyan/Blue
    LED_ERROR,          // Fast Blinking Red
    LED_WIFI_CONFIG     // Slow Blinking Blue
};

class RGBWL2812 {
public:
    // Constructor: Configure pin and number of LEDs
    RGBWL2812(int pin, int numLeds);

    void begin();
    void loop(); // Call this in the main loop for animations
    
    void setState(EvseLedState state);
    void setBrightness(uint8_t brightness);
    void setCustomColor(uint8_t r, uint8_t g, uint8_t b);

private:
    Adafruit_NeoPixel _strip;
    int _numLeds;
    EvseLedState _state;
    unsigned long _lastUpdate;
    int _animStep;
    bool _animDir;

    void setAll(uint32_t color);
    uint32_t Wheel(byte WheelPos);
};

#endif
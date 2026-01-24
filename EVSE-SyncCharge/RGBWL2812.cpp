#include "RGBWL2812.h"

RGBWL2812::RGBWL2812(int pin, int numLeds)
    : _strip(numLeds, pin, NEO_GRB + NEO_KHZ800), 
      _numLeds(numLeds), 
      _state(LED_OFF), 
      _lastUpdate(0), 
      _animStep(0), 
      _animDir(true)
{
}

void RGBWL2812::begin() {
    _strip.begin();
    _strip.show(); // Initialize all pixels to 'off'
    _strip.setBrightness(50); // Default brightness (0-255)
}

void RGBWL2812::setBrightness(uint8_t brightness) {
    _strip.setBrightness(brightness);
    _strip.show();
}

void RGBWL2812::setCustomColor(uint8_t r, uint8_t g, uint8_t b) {
    _state = LED_OFF; // Override state machine
    setAll(_strip.Color(r, g, b));
}

void RGBWL2812::setState(EvseLedState state) {
    if (_state == state) return;
    
    _state = state;
    _animStep = 0;
    _lastUpdate = 0; // Force immediate update
    
    // Handle static states immediately to avoid delay
    switch(_state) {
        case LED_OFF:       setAll(_strip.Color(0, 0, 0)); break;
        case LED_READY:     setAll(_strip.Color(0, 255, 0)); break; // Green
        case LED_CONNECTED: setAll(_strip.Color(255, 165, 0)); break; // Orange
        default: break; // Dynamic states handled in loop
    }
}

void RGBWL2812::loop() {
    unsigned long now = millis();

    switch (_state) {
        case LED_CHARGING:
            // Breathing Cyan Animation (approx 50Hz updates)
            if (now - _lastUpdate > 20) {
                _lastUpdate = now;
                if (_animDir) _animStep += 2; else _animStep -= 2;
                
                if (_animStep >= 200) _animDir = false;
                if (_animStep <= 10) _animDir = true;
                
                // Color(Red, Green, Blue) -> Cyan is (0, G, B)
                setAll(_strip.Color(0, _animStep, _animStep));
            }
            break;

        case LED_ERROR:
            // Fast Blink Red (250ms toggle)
            if (now - _lastUpdate > 250) {
                _lastUpdate = now;
                _animStep = !_animStep;
                if (_animStep) setAll(_strip.Color(255, 0, 0));
                else setAll(_strip.Color(0, 0, 0));
            }
            break;

        case LED_WIFI_CONFIG:
            // Slow Blink Blue (500ms toggle)
            if (now - _lastUpdate > 500) {
                _lastUpdate = now;
                _animStep = !_animStep;
                if (_animStep) setAll(_strip.Color(0, 0, 255));
                else setAll(_strip.Color(0, 0, 0));
            }
            break;
            
        case LED_BOOT:
             // Rainbow Cycle
             if (now - _lastUpdate > 20) {
                 _lastUpdate = now;
                 _animStep++;
                 if(_animStep > 255) _animStep = 0;
                 for(int i=0; i< _numLeds; i++) {
                     _strip.setPixelColor(i, Wheel((i * 256 / _numLeds + _animStep) & 255));
                 }
                 _strip.show();
             }
             break;

        default:
            break;
    }
}

void RGBWL2812::setAll(uint32_t color) {
    for(int i=0; i<_numLeds; i++) {
        _strip.setPixelColor(i, color);
    }
    _strip.show();
}

// Helper: Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t RGBWL2812::Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return _strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return _strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return _strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
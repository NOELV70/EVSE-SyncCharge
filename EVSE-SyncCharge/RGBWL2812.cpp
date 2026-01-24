/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the RGBWL2812 LED driver. Controls WS2812/NeoPixel LEDs
 *              to provide visual feedback for EVSE states (Ready, Charging, Error, etc.).
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "RGBWL2812.h"
#include <Preferences.h>

RGBWL2812::RGBWL2812(int pin)
    : _strip(8, pin, NEO_GRB + NEO_KHZ800), 
      _pin(pin),
      _currentState(LED_OFF_STATE), 
      _lastUpdate(0), 
      _animStep(0), 
      _animDir(true),
      _testMode(false),
      _testSequenceStart(0),
      _currentTestStep(-1)
{
    // Default Config
    _config.enabled = false;
    _config.numLeds = 8;
    _config.stateStandby   = {COL_GREEN, EFF_SOLID};
    _config.stateConnected = {COL_YELLOW, EFF_SOLID};
    _config.stateCharging  = {COL_CYAN, EFF_BREATH};
    _config.stateError     = {COL_RED, EFF_BLINK_FAST};
    _config.stateWifi      = {COL_BLUE, EFF_BLINK_SLOW};
    _config.stateBoot      = {COL_MAGENTA, EFF_RAINBOW};
    _config.stateSolarIdle = {COL_MAGENTA, EFF_BREATH};
}

void RGBWL2812::begin() {
    loadConfig();
    _strip.updateLength(_config.numLeds);
    _strip.setPin(_pin);
    _strip.begin();
    _strip.show(); // Initialize all pixels to 'off'
    _strip.setBrightness(50); // Default brightness (0-255)
}

void RGBWL2812::loadConfig() {
    Preferences prefs;
    prefs.begin("led_cfg", true);
    _config.enabled = prefs.getBool("en", false);
    _config.numLeds = prefs.getUShort("num", 8);
    
    auto loadState = [&](const char* key, LedStateSetting& s, LedColor dc, LedEffect de) {
        uint16_t val = prefs.getUShort(key, (uint16_t)((dc << 8) | de));
        s.color = (LedColor)((val >> 8) & 0xFF);
        s.effect = (LedEffect)(val & 0xFF);
    };

    loadState("s_stby", _config.stateStandby, COL_GREEN, EFF_SOLID);
    loadState("s_conn", _config.stateConnected, COL_YELLOW, EFF_SOLID);
    loadState("s_chg", _config.stateCharging, COL_CYAN, EFF_BREATH);
    loadState("s_err", _config.stateError, COL_RED, EFF_BLINK_FAST);
    loadState("s_wifi", _config.stateWifi, COL_BLUE, EFF_BLINK_SLOW);
    loadState("s_boot", _config.stateBoot, COL_MAGENTA, EFF_RAINBOW);
    loadState("s_solidle", _config.stateSolarIdle, COL_MAGENTA, EFF_BREATH);
    prefs.end();
}

void RGBWL2812::saveConfig() {
    Preferences prefs;
    prefs.begin("led_cfg", false);
    prefs.putBool("en", _config.enabled);
    prefs.putUShort("num", _config.numLeds);

    auto saveState = [&](const char* key, LedStateSetting& s) {
        uint16_t val = ((uint16_t)s.color << 8) | (uint16_t)s.effect;
        prefs.putUShort(key, val);
    };

    saveState("s_stby", _config.stateStandby);
    saveState("s_conn", _config.stateConnected);
    saveState("s_chg", _config.stateCharging);
    saveState("s_err", _config.stateError);
    saveState("s_wifi", _config.stateWifi);
    saveState("s_boot", _config.stateBoot);
    saveState("s_solidle", _config.stateSolarIdle);
    prefs.end();
    
    _strip.updateLength(_config.numLeds);
}

void RGBWL2812::updateConfig(LedSettings newConfig) {
    _config = newConfig;
    saveConfig();
}

void RGBWL2812::setState(EvseLedState state) {
    if (_currentState == state) return;
    _currentState = state;
    _animStep = 0;
    _lastUpdate = 0; // Force immediate update
}

void RGBWL2812::startTestSequence() {
    _testMode = true;
    _testSequenceStart = millis();
    _currentTestStep = -1;
}

void RGBWL2812::loop() {
    if (!_config.enabled && !_testMode) {
        // Ensure LEDs are off if disabled (unless testing)
        if (_strip.getPixelColor(0) != 0 && _strip.numPixels() > 0) {
            _strip.clear();
            _strip.show();
        }
        return;
    }

    unsigned long now = millis();
    LedStateSetting currentSetting;
    
    if (_testMode) {
        unsigned long elapsed = now - _testSequenceStart;
        int step = elapsed / 5000; // 5 seconds per state
        
        if (step != _currentTestStep) {
            _currentTestStep = step;
            _animStep = 0;
            _animDir = true;
            _lastUpdate = 0;
        }
        
        switch(step) {
            case 0: currentSetting = _config.stateStandby; break;
            case 1: currentSetting = _config.stateConnected; break;
            case 2: currentSetting = _config.stateCharging; break;
            case 3: currentSetting = _config.stateError; break;
            case 4: currentSetting = _config.stateWifi; break;
            case 5: currentSetting = _config.stateBoot; break;
            case 6: currentSetting = _config.stateSolarIdle; break;
            default: 
                _testMode = false; 
                _currentTestStep = -1;
                _animStep = 0;
                _lastUpdate = 0;
                return;
        }
    } else {
        switch (_currentState) {
            case LED_BOOT:        currentSetting = _config.stateBoot; break;
            case LED_READY:       currentSetting = _config.stateStandby; break;
            case LED_CONNECTED:   currentSetting = _config.stateConnected; break;
            case LED_CHARGING:    currentSetting = _config.stateCharging; break;
            case LED_ERROR:       currentSetting = _config.stateError; break;
            case LED_WIFI_CONFIG: currentSetting = _config.stateWifi; break;
            case LED_SOLAR_IDLE:  currentSetting = _config.stateSolarIdle; break;
            default:              currentSetting = {COL_OFF, EFF_OFF}; break;
        }
    }

    runEffect(currentSetting);
}

void RGBWL2812::runEffect(LedStateSetting setting) {
    unsigned long now = millis();
    uint32_t c = getColor(setting.color);

    if (setting.effect == EFF_OFF || setting.color == COL_OFF) {
        setAll(0);
        return;
    }

    if (setting.effect == EFF_SOLID) {
        setAll(c);
        return;
    }

    if (setting.effect == EFF_BLINK_SLOW || setting.effect == EFF_BLINK_FAST) {
        int interval = (setting.effect == EFF_BLINK_SLOW) ? 1000 : 250;
        if (now - _lastUpdate > interval) {
            _lastUpdate = now;
            _animStep = !_animStep;
            setAll(_animStep ? c : 0);
        }
        return;
    }

    if (setting.effect == EFF_BREATH) {
        if (now - _lastUpdate > 20) {
            _lastUpdate = now;
            if (_animDir) _animStep += 2; else _animStep -= 2;
            if (_animStep >= 255) _animDir = false;
            if (_animStep <= 5) _animDir = true;
            
            // Apply brightness scaling to color
            uint8_t r = (uint8_t)((c >> 16) & 0xFF);
            uint8_t g = (uint8_t)((c >> 8) & 0xFF);
            uint8_t b = (uint8_t)(c & 0xFF);
            
            r = (r * _animStep) >> 8;
            g = (g * _animStep) >> 8;
            b = (b * _animStep) >> 8;
            setAll(_strip.Color(r, g, b));
        }
        return;
    }

    if (setting.effect == EFF_RAINBOW) {
        if (now - _lastUpdate > 20) {
            _lastUpdate = now;
            _animStep++;
            if(_animStep > 255) _animStep = 0;
            for(int i=0; i< _config.numLeds; i++) {
                _strip.setPixelColor(i, Wheel((i * 256 / _config.numLeds + _animStep) & 255));
            }
            _strip.show();
        }
        return;
    }

    if (setting.effect == EFF_KNIGHT_RIDER) {
        if (now - _lastUpdate > 40) {
            _lastUpdate = now;
            _strip.clear();
            
            if (_animDir) _animStep++; else _animStep--;
            if (_animStep >= _config.numLeds - 1) _animDir = false;
            if (_animStep <= 0) _animDir = true;

            _strip.setPixelColor(_animStep, c);
            // Add trail
            if (_animDir && _animStep > 0) _strip.setPixelColor(_animStep - 1, _strip.Color(((c>>16)&0xFF)/4, ((c>>8)&0xFF)/4, (c&0xFF)/4));
            else if (!_animDir && _animStep < _config.numLeds - 1) _strip.setPixelColor(_animStep + 1, _strip.Color(((c>>16)&0xFF)/4, ((c>>8)&0xFF)/4, (c&0xFF)/4));

            _strip.show();
        }
        return;
    }

    if (setting.effect == EFF_CHASE) {
        if (now - _lastUpdate > 50) {
            _lastUpdate = now;
            _animStep++;
            if (_animStep >= _config.numLeds) _animStep = 0;
            
            _strip.clear();
            _strip.setPixelColor(_animStep, c);
            _strip.show();
        }
        return;
    }
}

void RGBWL2812::setAll(uint32_t color) {
    for(int i=0; i<_config.numLeds; i++) {
        _strip.setPixelColor(i, color);
    }
    _strip.show();
}

uint32_t RGBWL2812::getColor(LedColor c) {
    switch(c) {
        case COL_RED:     return _strip.Color(255, 0, 0);
        case COL_GREEN:   return _strip.Color(0, 255, 0);
        case COL_BLUE:    return _strip.Color(0, 0, 255);
        case COL_YELLOW:  return _strip.Color(255, 200, 0);
        case COL_CYAN:    return _strip.Color(0, 255, 255);
        case COL_MAGENTA: return _strip.Color(255, 0, 255);
        case COL_WHITE:   return _strip.Color(255, 255, 255);
        default:          return 0;
    }
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


// LED 
constexpr int PIN_GRB_LED_OUT    = 22;

RGBWL2812 led(PIN_GRB_LED_OUT);
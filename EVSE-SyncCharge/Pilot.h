/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the Pilot driver. Defines J1772 constants, PWM configuration,
 *              ADC settings, and the Pilot class interface for vehicle state detection.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "sdkconfig.h"
#include "EvseTypes.h" 

// =========================
// Constants - OFFICIAL SAE J1772 VALUES
// =========================

// Voltage divider scale factor (Matches your hardware) ( 5K6 to +3v3 , 4K7 to gnd , 15K in seial with input signal ( output volatge following opamp) )
const float ZERO_OFFSET_MV = 1200.0f;   // Vx when V2=0V
const float SCALE = 6.90f;           // mV_out per mV_in



// Current limits
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 80.0f;

// J1772 PWM Conversion Constants
constexpr float J1772_LOW_RANGE_MAX_AMPS   = 51.0f;
constexpr float J1772_LOW_RANGE_MAX_DUTY   = 85.0f;
constexpr float J1772_LOW_RANGE_FACTOR     = 0.6f;
constexpr float J1772_HIGH_RANGE_FACTOR    = 2.5f;
constexpr float J1772_HIGH_RANGE_OFFSET    = 64.0f;

// Voltage thresholds (MilliVolts) based on SAE J1772 states after voltage divider
// State A: +12V (No vehicle) -> ~10.6V after divider
// State B: +9V  (Vehicle detected, not ready) -> ~8.0V after divider  
// State C: +6V  (Vehicle ready, charging) -> ~5.0V after divider
// State D: +3V  (Ventilation required) -> ~2.0V after divider
// State E/F: 0V/-12V (No power / Fault)
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 10600;  // J1772 State A threshold
constexpr int VOLTAGE_STATE_CONNECTED     =  8000;  // J1772 State B threshold
constexpr int VOLTAGE_STATE_READY         =  5000;  // J1772 State C threshold
constexpr int VOLTAGE_STATE_VENTILATION   =  2000;  // J1772 State D threshold
constexpr int VOLTAGE_STATE_N12V_THRESHOLD = 1000;  // Diode check: verify -12V swing present



/* =========================
 * PWM Configuration
 * ========================= */
#if CONFIG_IDF_TARGET_ESP32
constexpr int PIN_PILOT_PWM_OUT    = 27;
constexpr int PIN_PILOT_IN         = 36;
#elif CONFIG_IDF_TARGET_ESP32S3
constexpr int PIN_PILOT_PWM_OUT    = 14;
// GPIO 36 is NOT an ADC pin on ESP32-S3. We use GPIO 1 (ADC1_CH0) as default.
constexpr int PIN_PILOT_IN         = 1;
#endif

constexpr int PILOT_PWM_FREQ       = 1000;

constexpr int PILOT_PWM_RESOLUTION = 12;
constexpr int PILOT_PWM_MAX_DUTY   = (1 << PILOT_PWM_RESOLUTION) - 1;

/* =========================
 * Analog Sampling Configuration
 * ========================= */
// Sample for 2 full PWM periods (2ms total) to ensure we hit the peaks
constexpr unsigned long PILOT_SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ;



#define RAW_AD_USE 1 

#if RAW_AD_USE
#define USE_CONTINUAL_AD_READS 1 // USE DMA AD SAMPLING ! 
#include <hal/adc_types.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h> // Required for DMA
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#ifdef USE_CONTINUAL_AD_READS
// DMA Configuration for 40kHz
#define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1
#if CONFIG_IDF_TARGET_ESP32
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1
#endif

#if CONFIG_IDF_TARGET_ESP32S3
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#endif


constexpr int ADC_SAMPLE_RATE_HZ = 40 * PILOT_PWM_FREQ; 

// How many samples occur during our desired duration (2ms)?
// (2,000us / 1,000,000us) * 40,000Hz = 80 samples
constexpr int REQUIRED_SAMPLES = (PILOT_SAMPLE_DURATION_US * ADC_SAMPLE_RATE_HZ) / 1000000;

// The DMA engine requires the frame size to be a multiple of SOC_ADC_DIGI_DATA_BYTES_PER_CONV 
// and usually prefers powers of 2 for efficiency.
// We'll set a buffer large enough to hold at least our REQUIRED_SAMPLES.
constexpr int ADC_SAMPLES_COUNT = 128; // Fixed count for the hardware buffer (Power of 2 preferred)
constexpr int ADC_READ_BYTE_LEN = ADC_SAMPLES_COUNT * 2; // 4 bytes per sample (SOC_ADC_DIGI_RESULT_BYTES)

#endif
#endif


class Pilot {
private:    
    int highVoltageMv = 0; 
    int lowVoltageMv = 0;
    float currentDutyPercent = 0.0f;
    bool pwmAttached = false;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_NOT_CONNECTED; 

#if RAW_AD_USE
    adc_channel_t _adc_channel;
    adc_cali_handle_t cali_handle = nullptr;
    
    #if USE_CONTINUAL_AD_READS
    adc_continuous_handle_t _continuous_handle = nullptr;
    uint8_t* _dma_buffer = nullptr;
    #else
    adc_oneshot_unit_handle_t _adc_handle; 
    #endif
#endif

public:
    Pilot();
    ~Pilot();
    void begin();
    void standby();
    void disable();
    void stop();
    void currentLimit(float amps);
    VEHICLE_STATE_T read();
    float getVoltage();
    float getPwmDuty();
    float ampsToDuty(float amps);
    float dutyToAmps(float duty);

private:
    int analogReadMax();
    float convertMv(int adMv);
};

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer);

#endif

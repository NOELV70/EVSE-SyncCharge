/*****************************************************************************
 * @file Pilot.h
 * @brief Pilot driver API and vehicle state helpers.
 ******************************************************************************/

#ifndef PILOT_H_
#define PILOT_H_

#include <Arduino.h>
#include "EvseTypes.h" 

// =========================
// Constants - OFFICIAL SAE J1772 VALUES
// =========================

// Voltage divider scale factor (Matches your hardware)
constexpr float PILOT_VOLTAGE_SCALE = ((1200.0+3300.0)/1200.0)*1.2; 

// Current limits
constexpr float MIN_CURRENT = 6.0f;
constexpr float MAX_CURRENT = 80.0f;

// J1772 PWM Conversion Constants
constexpr float J1772_LOW_RANGE_MAX_AMPS   = 51.0f;
constexpr float J1772_LOW_RANGE_MAX_DUTY   = 85.0f;
constexpr float J1772_LOW_RANGE_FACTOR     = 0.6f;
constexpr float J1772_HIGH_RANGE_FACTOR    = 2.5f;
constexpr float J1772_HIGH_RANGE_OFFSET    = 64.0f;

// Voltage thresholds (MilliVolts) based on board_config style
constexpr int VOLTAGE_STATE_NOT_CONNECTED = 11000; 
constexpr int VOLTAGE_STATE_CONNECTED     =  8000;
constexpr int VOLTAGE_STATE_READY         =  5000;
constexpr int VOLTAGE_STATE_VENTILATION   =  2000;
constexpr int VOLTAGE_STATE_N12V_THRESHOLD = 1000; // Threshold to verify -12V swing



/* =========================
 * PWM Configuration
 * ========================= */
constexpr int PIN_PILOT_PWM_OUT    = 27;
constexpr int PIN_PILOT_IN         = 36;

constexpr int PILOT_PWM_FREQ       = 1000;

constexpr int PILOT_PWM_RESOLUTION = 12;
constexpr int PILOT_PWM_MAX_DUTY   = (1 << PILOT_PWM_RESOLUTION) - 1;

/* =========================
 * Analog Sampling Configuration
 * ========================= */
// Sample for 2 full PWM periods (2ms total) to ensure we hit the peaks
constexpr unsigned long PILOT_SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ;



#define RAW_AD_USE 1 
#define USE_CONTINUAL_AD_READS 1 // <--- NEW TOGGLE

#if RAW_AD_USE
#include <hal/adc_types.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h> // Required for DMA
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#ifdef USE_CONTINUAL_AD_READS
// DMA Configuration for 40kHz
#define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1


constexpr int ADC_SAMPLE_RATE_HZ = 40 * PILOT_PWM_FREQ; 

// How many samples occur during our desired duration (2ms)?
// (2,000us / 1,000,000us) * 40,000Hz = 80 samples
constexpr int REQUIRED_SAMPLES = (PILOT_SAMPLE_DURATION_US * ADC_SAMPLE_RATE_HZ) / 1000000;

// The DMA engine requires the frame size to be a multiple of SOC_ADC_DIGI_DATA_BYTES_PER_CONV 
// and usually prefers powers of 2 for efficiency.
// We'll set a buffer large enough to hold at least our REQUIRED_SAMPLES.
constexpr int ADC_SAMPLES_COUNT = 256; // Fixed count for the hardware buffer
constexpr int ADC_READ_BYTE_LEN = ADC_SAMPLES_COUNT * SOC_ADC_DIGI_RESULT_BYTES;

#endif
#endif


class Pilot {
private:    
    int highVoltageMv = 0; 
    int lowVoltageMv = 0;
    float currentDutyPercent = 0.0f;
    bool pwmAttached = false;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_ERROR; 

#if RAW_AD_USE
    adc_channel_t _adc_channel;
    adc_cali_handle_t cali_handle = nullptr;
    
    #if USE_CONTINUAL_AD_READS
    adc_continuous_handle_t _continuous_handle = nullptr;
    #else
    adc_oneshot_unit_handle_t _adc_handle; 
    #endif
#endif

public:
    Pilot();
    void begin();
    void standby();
    void disable();
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

#include "hal/adc_types.h"
#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <esp32-hal-ledc.h>
#include <limits.h> // Added for INT_MAX


#include "EvseLogger.h"
#include "Pilot.h"

// Constructor - Clean and empty because variables are initialized in the header
Pilot::Pilot() 
{
#if USE_CONTINUAL_AD_READS && RAW_AD_USE
    // Allocate buffer in constructor as requested
    // Use MALLOC_CAP_INTERNAL to ensure DMA-capable memory (SRAM), avoiding PSRAM crashes on S3.
    _dma_buffer = (uint8_t*)heap_caps_malloc(ADC_READ_BYTE_LEN, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

Pilot::~Pilot()
{
#if USE_CONTINUAL_AD_READS && RAW_AD_USE
    if (_dma_buffer) {
        free(_dma_buffer);
        _dma_buffer = nullptr;
    }
#endif
}

void Pilot::begin()
{
    logger.info("[PILOT] - begin");
// 1. Standard Arduino Setup
#ifndef  USE_CONTINUAL_AD_READS
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_PILOT_IN, ADC_11db); // DB_11 is now DB_12 in IDF 5
    pinMode(PIN_PILOT_IN, INPUT);
#endif

#if RAW_AD_USE
    int ch = digitalPinToAnalogChannel(PIN_PILOT_IN);
    if (ch < 0) { logger.error("[PILOT] Invalid ADC Pin"); return; }
    _adc_channel = (adc_channel_t)ch;

    #if USE_CONTINUAL_AD_READS
        // 1. Setup DMA Handle
        adc_continuous_handle_cfg_t adc_config = {
            .max_store_buf_size = ADC_READ_BYTE_LEN * 4, // Must be a multiple of conv_frame_size
            .conv_frame_size = ADC_READ_BYTE_LEN ,
        };
        if (adc_continuous_new_handle(&adc_config, &_continuous_handle) != ESP_OK) {
            logger.error("[PILOT] Failed to create ADC Continuous Handle");
            return;
        }

        // 2. Configure 20kHz background sampling
        adc_continuous_config_t dig_cfg = {
            .sample_freq_hz = ADC_SAMPLE_RATE_HZ, 
            .conv_mode = ADC_CONV_MODE,
            .format = ADC_OUTPUT_TYPE,
        };

        adc_digi_pattern_config_t adc_pattern = {
            .atten = ADC_ATTEN_DB_12,
            .channel = (uint8_t)_adc_channel,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        };

        dig_cfg.pattern_num = 1;
        dig_cfg.adc_pattern = &adc_pattern;
        
        if (adc_continuous_config(_continuous_handle, &dig_cfg) != ESP_OK) {
            logger.error("[PILOT] Failed to configure ADC");
            adc_continuous_deinit(_continuous_handle);
            _continuous_handle = nullptr;
            return;
        }
        if (adc_continuous_start(_continuous_handle) != ESP_OK) {
            logger.error("[PILOT] Failed to start ADC");
            adc_continuous_deinit(_continuous_handle);
            _continuous_handle = nullptr;
            return;
        }
        logger.info("[PILOT] DMA Continuous ADC Started (40kHz)");

    #else
        // Legacy Oneshot Setup
        adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
        adc_oneshot_new_unit(&init_config, &_adc_handle);
        adc_oneshot_chan_cfg_t config = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
        adc_oneshot_config_channel(_adc_handle, _adc_channel, &config);
    #endif

    // Calibration Setup (Shared)
    #if CONFIG_IDF_TARGET_ESP32
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    #endif 
    #if CONFIG_IDF_TARGET_ESP32S3
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    #endif
#endif
    logger.info("[PILOT] ADC and PWM Pins configured");
}

void Pilot::standby()
{
    if(pwmAttached) {
        logger.debug("[PILOT] Detaching PWM for Standby (Static HIGH)");
        pwmAttached = false;
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);
}

void Pilot::disable()
{
    standby();
}

void Pilot::stop()
{
    standby(); // Force PWM to 12V (Safety)
#if USE_CONTINUAL_AD_READS && RAW_AD_USE
    if (_continuous_handle) {
        logger.info("[PILOT] Stopping ADC for OTA...");
        adc_continuous_stop(_continuous_handle);
        adc_continuous_deinit(_continuous_handle);
        _continuous_handle = nullptr;
    }
#endif
}

void Pilot::currentLimit(float amps)
{
    float dutyPercent = ampsToDuty(amps);
    currentDutyPercent = dutyPercent;

    uint32_t dutyCounts = (uint32_t)roundf((dutyPercent / 100.0f) * PILOT_PWM_MAX_DUTY);

    if(!pwmAttached) {
        ledcAttach(PIN_PILOT_PWM_OUT, PILOT_PWM_FREQ, PILOT_PWM_RESOLUTION);
        pwmAttached = true;
    }
    ledcWrite(PIN_PILOT_PWM_OUT, dutyCounts);
}



VEHICLE_STATE_T Pilot::read() {
    int highRaw = 0;
    int lowRaw = INT_MAX;
    int counts_ = 0;
#if USE_CONTINUAL_AD_READS && RAW_AD_USE
    if (!_dma_buffer) return lastVehicleState; // Safety check if allocation failed
    if (!_continuous_handle) return lastVehicleState; // Safety check if driver init failed

    uint32_t ret_num = 0;
    esp_err_t err;
    
    // Grab everything the DMA has collected since last call
    // Loop to drain the buffer completely (since loop runs every 20ms and DMA fills faster)
    while (true) {
        ret_num = 0;
        err = adc_continuous_read(_continuous_handle, _dma_buffer, ADC_READ_BYTE_LEN, &ret_num, 0);

        if (err != ESP_OK || ret_num == 0) {
            break;
        }

        for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t*)&_dma_buffer[i];
            #if CONFIG_IDF_TARGET_ESP32
            int val = p->type1.data;
            #endif 
            #if CONFIG_IDF_TARGET_ESP32S3
            int val = p->type2.data;
            #endif
            if (val > highRaw) highRaw = val;
            if (val < lowRaw)  lowRaw = val;
            counts_++;
        }
    }

    if (counts_ == 0) 
    {
        return lastVehicleState;
    }
   // logger.debugf("[PILOT] - DMA #samples : %d", counts_);
#else
    // Fallback to your existing Oneshot logic (Stuck at 49 samples)
    unsigned long startTime = micros();
    int val;
    while ((micros() - startTime) < PILOT_SAMPLE_DURATION_US) {
        #if RAW_AD_USE
            adc_oneshot_read(_adc_handle, _adc_channel, &val);
        #else
            val = analogReadMilliVolts(PIN_PILOT_IN);
        #endif
        if (val > highRaw) highRaw = val;
        if (val < lowRaw)  lowRaw = val;
        counts_++;
    }
    //logger.debugf("[PILOT] - MAN #samples : %d", counts_);
#endif


    // Calibration
#if RAW_AD_USE
    if (cali_handle) {
        int tHigh, tLow;
        adc_cali_raw_to_voltage(cali_handle, highRaw, &tHigh);
        adc_cali_raw_to_voltage(cali_handle, lowRaw, &tLow);
        highRaw = tHigh;
        lowRaw = tLow;
    }
#endif
    //logger.debugf("[PILOT] - samples - HI %d , LOW: %d", highRaw, lowRaw);
    
    // 2. Conversion
    highVoltageMv = (int)convertMv(highRaw);
    lowVoltageMv  = (int)convertMv(lowRaw);

    // 3. Temporary state determination
    VEHICLE_STATE_T detectedState;
    if (highVoltageMv >= VOLTAGE_STATE_NOT_CONNECTED)      detectedState = VEHICLE_NOT_CONNECTED;
    else if (highVoltageMv >= VOLTAGE_STATE_CONNECTED)     detectedState = VEHICLE_CONNECTED;
    else if (highVoltageMv >= VOLTAGE_STATE_READY)         detectedState = VEHICLE_READY;
    else if (highVoltageMv >= VOLTAGE_STATE_VENTILATION)   detectedState = VEHICLE_READY_VENTILATION_REQUIRED;
    else                                                   detectedState = VEHICLE_NO_POWER;

    // Diode Check (State F)
    if (pwmAttached && detectedState != VEHICLE_NOT_CONNECTED) {
        if (lowVoltageMv > VOLTAGE_STATE_N12V_THRESHOLD) {
            detectedState = VEHICLE_ERROR;
        }
    }

    // 4. "Best of 3" Debouncing
    // We only update lastVehicleState if we see the same detectedState multiple times
    static VEHICLE_STATE_T candidateState = VEHICLE_ERROR;
    static int stabilityCounter = 0;

    if (detectedState == candidateState) {
        stabilityCounter++;
    } else {
        candidateState = detectedState;
        stabilityCounter = 0;
    }

    // Only commit to the change if it has been stable for 3 checks
    if (stabilityCounter >= 3 && candidateState != lastVehicleState) {
        lastVehicleState = candidateState;
        
        char stateBuf[50];
        vehicleStateToText(lastVehicleState, stateBuf);
        logger.debugf("[PILOT] Stable Change: %s (H:%dmV L:%dmV)", 
                      stateBuf, highVoltageMv, lowVoltageMv);
    }

    return lastVehicleState;
}

/* API & Helper Methods */
float Pilot::getVoltage() { return (float)highVoltageMv / 1000.0f; }
float Pilot::getPwmDuty() { return currentDutyPercent; }
float Pilot::convertMv(int adMv) {
    return ((float)adMv - ZERO_OFFSET_MV) * SCALE;
}

float Pilot::ampsToDuty(float amps) {
    amps = constrain(amps, MIN_CURRENT, MAX_CURRENT);
    if (amps <= J1772_LOW_RANGE_MAX_AMPS) return amps / J1772_LOW_RANGE_FACTOR;
    return (amps / J1772_HIGH_RANGE_FACTOR) + J1772_HIGH_RANGE_OFFSET;
}

float Pilot::dutyToAmps(float duty) {
    if (duty <= J1772_LOW_RANGE_MAX_DUTY) return duty * J1772_LOW_RANGE_FACTOR;
    return (duty - J1772_HIGH_RANGE_OFFSET) * J1772_HIGH_RANGE_FACTOR;
}

// Keep this for legacy calls if necessary, but logic is now inside read()
int Pilot::analogReadMax() { return highVoltageMv; }

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer)
{
    switch (vehicleState) {
        case VEHICLE_NOT_CONNECTED:              strcpy(buffer, "A: Standby"); break;
        case VEHICLE_CONNECTED:                  strcpy(buffer, "B: Vehicle Detected"); break;
        case VEHICLE_READY:                      strcpy(buffer, "C: Charging"); break;
        case VEHICLE_READY_VENTILATION_REQUIRED: strcpy(buffer, "D: Ventilation Req"); break;
        case VEHICLE_NO_POWER:                   strcpy(buffer, "E: No Power"); break;
        case VEHICLE_ERROR:                      strcpy(buffer, "F: Fault/Error"); break;
        default:                                 strcpy(buffer, "Unknown"); break;
    }
}
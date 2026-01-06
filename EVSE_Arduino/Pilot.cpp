/*****************************************************************************
 * @file Pilot.cpp
 * @brief Pilot signal driver for SAE J1772 PWM and vehicle state sensing.
 *
 * @details
 * Controls PWM on the pilot pin, reads pilot voltage for vehicle state
 * detection and exposes convenience helpers used by the EVSE charging logic.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#include <Arduino.h>
#include <cstring>
#include <cmath>

#include <esp32-hal-ledc.h>

#include "EvseLogger.h"
#include "Pilot.h"

/* =========================
 * PWM Configuration
 * ========================= */
constexpr int PIN_PILOT_PWM_OUT    = 27;   // PWM output
constexpr int PILOT_PWM_FREQ       = 1000; // 1 kHz
constexpr int PILOT_PWM_RESOLUTION = 12;   // 12-bit (0–4095)
constexpr int PILOT_PWM_CHANNEL = 0; // LEDC channel (kept for compatibility)

/* =========================
 * Analog Sampling Configuration
 * ========================= */
constexpr unsigned long SAMPLE_DURATION_US = (2 * 1000000) / PILOT_PWM_FREQ; // Sample for 2 PWM periods

/* =========================
 * IRQ Sampling (25x PWM frequency - parametric)
 * ========================= */
#ifdef USE_PILOT_IRQ
constexpr uint32_t PILOT_ISR_FREQ_HZ = PILOT_PWM_FREQ * 25;  // 25kHz @ 1kHz PWM, scales automatically
constexpr uint32_t PILOT_ISR_SAMPLES_PER_WINDOW = (PILOT_ISR_FREQ_HZ / PILOT_PWM_FREQ) * 2;  // Samples in 2 PWM periods

volatile int16_t pilot_sample_max = 0;
volatile uint32_t pilot_sample_count = 0;
volatile int16_t pilot_last_peak = 0;
hw_timer_t* pilot_timer = nullptr;

void IRAM_ATTR pilot_timer_isr() {
    int adc = analogRead(PIN_PILOT_IN);
    if (adc > pilot_sample_max) 
        pilot_sample_max = adc;
    
    pilot_sample_count++;
    if (pilot_sample_count >= PILOT_ISR_SAMPLES_PER_WINDOW) {
        pilot_last_peak = pilot_sample_max;  // Save peak
        pilot_sample_max = 0;                // Reset for next window
        pilot_sample_count = 0;
    }
}
#endif

/* =========================
 * Analog input pin
 * ========================= */
constexpr int PIN_PILOT_IN = 36; // ADC1 channel 0

/* =========================
 * Helper function: analog read max
 * ========================= */
int analogReadMax(uint8_t pinNumber)
{
   int maxVal = 0;
   // Sample for exactly 2 PWM periods to capture peak reliably
   unsigned long startTime = micros();
   unsigned long duration = 0;
   
   while (duration < SAMPLE_DURATION_US) {
       int val = analogRead(pinNumber);
       if (val > maxVal) maxVal = val;
       duration = micros() - startTime;
   }
   
   //logger.debugf("[PILOT] analogReadMax sampled for %lu us, result: %d", duration, maxVal);
   return maxVal;
}
// Constructor
Pilot::Pilot() 
{
    voltage = 0.0f;
    currentDutyPercent = 0.0f;
}

void Pilot::disable()
{
    standby();
}

void Pilot::standby()
{
    logger.debug("[PILOT] Setting pilot HIGH (standby)");
    if(pwmAttached)
    {
        pwmAttached=false;
        // Stop PWM on this pin (toolchain provides ledcDetach)
        ledcDetach(PIN_PILOT_PWM_OUT);
    }    
    // Set pin as HIGH output
    pinMode(PIN_PILOT_PWM_OUT, OUTPUT);
    digitalWrite(PIN_PILOT_PWM_OUT, HIGH);}

void Pilot::currentLimit(float amps)
{
    amps = constrain(amps, MIN_CURRENT, MAX_CURRENT);

    float dutyPercent;

    if (amps <= 51.0f)
        dutyPercent = amps / 0.6f;
    else
        dutyPercent = (amps / 2.5f) + 64.0f;

    dutyPercent = constrain(dutyPercent, 0.0f, 100.0f);

    currentDutyPercent = dutyPercent; // <-- store duty percent

    uint32_t maxDuty = (1UL << PILOT_PWM_RESOLUTION) - 1;
    uint32_t dutyCounts = (uint32_t)roundf((dutyPercent / 100.0f) * maxDuty);

    logger.infof("[PILOT] Pilot current: %.2f A | Duty: %.2f %% | Counts: %lu", amps, dutyPercent, dutyCounts);

    // ESP32 PWM setup
    if(!pwmAttached)
    {
        // Some ESP32 Arduino cores expose pin-based ledcAttach/ledcDetach API.
        // Use ledcAttach(pin, freq, resolution) if available in this toolchain.
        ledcAttach(PIN_PILOT_PWM_OUT, PILOT_PWM_FREQ, PILOT_PWM_RESOLUTION);
        pwmAttached=true;
    }
    // Use pin-based ledcWrite when channel-based API isn't available.
    ledcWrite(PIN_PILOT_PWM_OUT, dutyCounts);
}


float Pilot::readPin()
{
#ifdef USE_PILOT_IRQ
    // IRQ mode: read pre-sampled peak from ISR (frequency = 25x PWM_FREQ)
    int pinValue = pilot_last_peak;
    //logger.debugf("[PILOT] IRQ mode: using peak=%d", pinValue);
#else
    // Loop mode: sample on-demand during 2 PWM periods
    int pinValue = analogReadMax(PIN_PILOT_IN);
#endif

    // 1. Convert Raw ADC to Volts at the Pin
    float pinVoltage = (float)pinValue * (ADC_VREF / (float)ADC_MAX_VALUE);
    // 2. Convert Pin Volts to Actual Signal Volts (-12 to +12)
    // Formula derived from: V_sig = pinVoltage * PILOT_VOLTAGE_SCALE + PILOT_VOLTAGE_OFFSET
    this->voltage = (pinVoltage * PILOT_VOLTAGE_SCALE) + PILOT_VOLTAGE_OFFSET;    
    logger.debugf("[PILOT] Analog: raw=%d, pinVoltage=%.2f, voltage=%.2f", pinValue, pinVoltage, this->voltage);
    return this->voltage;
}

float Pilot::getVoltage()
{
    logger.debugf("[PILOT] getVoltage -> %.2f V", this->voltage);
    return this->voltage;
}

VEHICLE_STATE_T Pilot::read()
{
    float voltage = floor(this->readPin());

    VEHICLE_STATE_T state;
    if (voltage >= 11) state = VEHICLE_NOT_CONNECTED;
    else if (voltage >= 8)  state = VEHICLE_CONNECTED;
    else if (voltage >= 5)  state = VEHICLE_READY;
    else if (voltage >= 2)  state = VEHICLE_READY_VENTILATION_REQUIRED;
    else if (voltage >= 0)  state = VEHICLE_NO_POWER;
    else state = VEHICLE_ERROR;
    
    // Log only on state change
    if (state != lastVehicleState) {
        lastVehicleState = state;
        char stateBuf[50];
        vehicleStateToText(state, stateBuf);
        logger.debugf("[PILOT] Read: voltage=%.0f -> %s", voltage, stateBuf);
    }
    
    return state;
}

float Pilot::getPwmDuty() {
    //logger.debugf("[PILOT] getPwmDuty -> %.2f %%", currentDutyPercent);
    return currentDutyPercent; // store last duty calculated in currentLimit()
}

void vehicleStateToText(VEHICLE_STATE_T vehicleState, char* buffer)
{
    switch (vehicleState)
    {
        case VEHICLE_NOT_CONNECTED:              strcpy(buffer, "Not connected"); break;
        case VEHICLE_CONNECTED:                  strcpy(buffer, "Connected, not ready"); break;
        case VEHICLE_READY:                      strcpy(buffer, "Ready"); break;
        case VEHICLE_READY_VENTILATION_REQUIRED: strcpy(buffer, "Ready, ventilation required"); break;
        case VEHICLE_NO_POWER:                   strcpy(buffer, "No power"); break;
        case VEHICLE_ERROR:                      strcpy(buffer, "Error"); break;
        default:                                 strcpy(buffer, "Unknown"); break;
    }
}

#ifdef USE_PILOT_IRQ
/* =========================
 * IRQ Timer Setup/Teardown
 * ========================= */
void Pilot::initPilotIrq()
{
    // Timer frequency = 25 * PWM_FREQ (scales automatically)
    // ESP32 uses 80MHz APB clock
    const uint32_t PRESCALER = 80;  // 80MHz / 80 = 1MHz clock
    const uint32_t COUNTER_LIMIT = 1000000 / PILOT_ISR_FREQ_HZ;  // 1MHz / ISR_FREQ
    
    pilot_timer = timerBegin(0, PRESCALER, true);  // Use timer 0, prescaler 80, count up
    timerAttachInterrupt(pilot_timer, &pilot_timer_isr, true);  // Edge-triggered
    timerAlarmWrite(pilot_timer, COUNTER_LIMIT, true);  // Reload on match
    timerAlarmEnable(pilot_timer);
    
    logger.infof("[PILOT] IRQ sampling initialized: %lu Hz, %lu samples/window", 
                 PILOT_ISR_FREQ_HZ, PILOT_ISR_SAMPLES_PER_WINDOW);
}

void Pilot::deinitPilotIrq()
{
    if (pilot_timer != nullptr) {
        timerAlarmDisable(pilot_timer);
        timerDetachInterrupt(pilot_timer);
        timerEnd(pilot_timer);
        pilot_timer = nullptr;
    }
    logger.info("[PILOT] IRQ sampling disabled");
}
#endif

/* Key Observations from the Simulation:Pin C (The Car): Notice that when plugged in, the car sees slightly less than $9\text{V}$ and $6\text{V}$ (it sees $8.45\text{V}$ and $5.5\text{V}$). This is because your $12\text{k}\Omega$ sensing resistor (R4) adds a tiny bit of extra load. This is perfectly fine—car chargers have a tolerance, and anything within $\pm0.5\text{V}$ is usually accepted as valid.The -12V Floor: As you can see, the negative voltage at Pin C and Pin B stays exactly the same in every state. This confirms that the car’s diode is successfully blocking the negative swing, so the vehicle resistors don't affect it.MCU Safety: Your Pin B never goes negative. Even when the Op-amp hits $-12\text{V}$, Pin B stays at $+0.13\text{V}$ thanks to the R2 pull-up. This is safe for any $3.3\text{V}$ MCU.ADC Resolution: You have a clear $500\text{-unit}$ gap between being disconnected, plugged in, and charging. This makes it very easy to write stable software.Recommendation for your LogicYour code should detect the Peak of the signal.If $Peak > 3800$: State A (Disconnected)If $Peak$ is between $3300$ and $3700$: State B (Connected)If $Peak$ is between $2800$ and $3200$: State C (Charging)If the $Minimum$ value ever goes below $50$: Something is wrong (Negative spike). */
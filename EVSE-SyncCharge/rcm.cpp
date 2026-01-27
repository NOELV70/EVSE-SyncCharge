/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the Residual Current Monitor (RCM) driver. Handles
 *              interrupt-based fault detection and periodic self-testing logic.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "Rcm.h"
#include "EvseLogger.h"

/* =========================
 * Hardware constants
 * ========================= */
constexpr int PIN_RCM_TEST = 26; // Digital output to trigger RCM test coil
constexpr int PIN_RCM_IN   = 25; // Digital input from RCM (Requires internal Pull-Down)

static SemaphoreHandle_t rcmSemaphore = NULL;

static void IRAM_ATTR rcmIsr()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (rcmSemaphore != NULL) {
        xSemaphoreGiveFromISR(rcmSemaphore, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

Rcm::Rcm()
{
}

void Rcm::begin()
{
    logger.info("[RCM] Initializing Residual Current Monitor...");

    rcmSemaphore = xSemaphoreCreateBinary();
    if (rcmSemaphore == NULL) return;

    // Configure Input Pin
    // Note: We use INPUT_PULLDOWN to match original logic. 
    // Ensure PIN_RCM_IN is a GPIO that supports internal pull-down (GPIO 0-33).
    pinMode(PIN_RCM_IN, INPUT_PULLDOWN);

    // Configure Test Pin
    pinMode(PIN_RCM_TEST, OUTPUT);
    digitalWrite(PIN_RCM_TEST, LOW);

    // Attach Interrupt
    attachInterrupt(digitalPinToInterrupt(PIN_RCM_IN), rcmIsr, RISING);
    
    logger.infof("[RCM] Configured: IN=%d, TEST=%d", PIN_RCM_IN, PIN_RCM_TEST);
}

bool Rcm::selfTest()
{
    logger.info("[RCM] Starting Self-Test...");

    if (rcmSemaphore == NULL) return false;

    // Clear any pending semaphore
    xSemaphoreTake(rcmSemaphore, 0);

    // Trigger Test Signal
    digitalWrite(PIN_RCM_TEST, HIGH);
    
    // Wait for RCM to trip (Timeout 500ms)
    bool success = xSemaphoreTake(rcmSemaphore, pdMS_TO_TICKS(500));
    
    // Reset Test Signal
    digitalWrite(PIN_RCM_TEST, LOW);

    if (success) {
        logger.info("[RCM] Self-Test PASSED");
    } else {
        logger.error("[RCM] Self-Test FAILED (Timeout)");
    }

    return success;
}

bool Rcm::isTriggered()
{
    if (rcmSemaphore == NULL) return false;

    // Check if interrupt fired
    if (xSemaphoreTake(rcmSemaphore, 0) == pdTRUE) {
        // Debounce / Noise filter
        delay(1);
        if (digitalRead(PIN_RCM_IN) == HIGH) {
            return true;
        }
    }
    return false;
}
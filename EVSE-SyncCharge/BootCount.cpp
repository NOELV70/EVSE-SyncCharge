/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the BootCount class. Manages a persistent counter in
 *              RTC memory (survives soft reboots/crashes) to detect boot loops.
 *              If the device crashes repeatedly within a short window, it triggers a lockout.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "BootCount.h"
#include "EvseLogger.h"
#include <esp_system.h>
#include <esp_attr.h>

#define BOOT_MAGIC 0xBEEF
#define BOOT_LIMIT 5
#define STABILITY_MS 300000 // 5 minutes

// RTC Memory persists across Reboots/Crashes but NOT Power Cycles.
RTC_NOINIT_ATTR uint32_t g_bootRegister; // High 16: Magic, Low 16: Count

BootCount bootCount;

BootCount::BootCount() {}

void BootCount::begin() {

    uint16_t magic = (uint16_t)((g_bootRegister >> 16) & 0xFFFF);
    uint16_t count = (uint16_t)(g_bootRegister & 0xFFFF);

    // Check Magic Signature
    if (magic != BOOT_MAGIC) {
        count = 1; // Cold boot or invalid memory -> Reset
        g_bootRegister = ((uint32_t)BOOT_MAGIC << 16) | count;
        logger.infof("[BOOT] Boot Counter BAD MAGIC");
    } else {
         count++; // Increment counter
        g_bootRegister = ((uint32_t)BOOT_MAGIC << 16) | count;
    }

    logger.infof("[BOOT] Boot Counter (RTC): %d", count);

    if (count > BOOT_LIMIT) {
        logger.error("[BOOT] CRITICAL: Boot Loop Detected! Safety Lockout Active.");
    }
}

void BootCount::loop() {
    uint16_t count = (uint16_t)(g_bootRegister & 0xFFFF);
    // If system stays up for STABILITY_MS, reset the counter
    if (count > 0 && millis() > STABILITY_MS) {
        // Reset count to 0, keep magic
        g_bootRegister = ((uint32_t)BOOT_MAGIC << 16); 
        logger.info("[BOOT] System stable for 5 minutes. Boot counter reset.");
    }
}

bool BootCount::IsBootCountHigh() const {
    uint16_t count = (uint16_t)(g_bootRegister & 0xFFFF);
    return count > BOOT_LIMIT;
}

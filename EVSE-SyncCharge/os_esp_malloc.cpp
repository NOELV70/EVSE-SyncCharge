/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of memory allocation wrappers for ESP32. Provides helper 
 *              functions to prioritize external PSRAM for large buffers with automatic 
 *              fallback to internal SRAM.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "os_esp_malloc.h"
#include <esp_heap_caps.h>
#include <stdlib.h>

void* os_esp_malloc_large(size_t size) {
    // 1. Try to allocate in PSRAM (SPIRAM) first
    //    MALLOC_CAP_SPIRAM: Use external RAM
    //    MALLOC_CAP_8BIT:   Ensure memory is byte-accessible (required for char arrays/buffers)
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    // 2. Fallback to internal RAM if PSRAM allocation failed (or if no PSRAM present)
    if (ptr == NULL) {
        ptr = malloc(size);
    }
    
    return ptr;
}

void os_esp_free(void* ptr) {
    free(ptr);
}

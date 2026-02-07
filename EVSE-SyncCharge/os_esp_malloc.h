/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Memory allocation wrappers for ESP32. Provides helper functions to 
 *              prioritize external PSRAM for large buffers with automatic fallback 
 *              to internal SRAM.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef OS_ESP_MALLOC_H
#define OS_ESP_MALLOC_H

#include <stddef.h>

/**
 * @brief Allocates memory, preferring external PSRAM (SPIRAM) if available.
 *        If PSRAM allocation fails or is not available, it falls back to internal RAM.
 * 
 * @param size Size of memory to allocate in bytes.
 * @return void* Pointer to allocated memory, or NULL if both allocations fail.
 */
void* os_esp_malloc_large(size_t size);
void os_esp_free(void* ptr);

#endif // OS_ESP_MALLOC_H
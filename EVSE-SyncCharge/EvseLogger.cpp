/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the global logger instance. Provides a centralized
 *              logging facility for the firmware.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "EvseLogger.h"

// Global logger instance
EvseLogger logger(Serial);

/*****************************************************************************
 * @file EvseLogger.cpp
 * @brief Global logger instance implementation.
 *
 * @details
 * Creates the global `EvseLogger logger` instance used across the firmware
 * for structured logging.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#include "EvseLogger.h"

// Global logger instance
EvseLogger logger(Serial);

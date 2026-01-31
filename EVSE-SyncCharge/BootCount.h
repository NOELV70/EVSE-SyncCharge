/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header for the BootCount class. Provides boot loop detection and protection
 *              using RTC memory to track crash frequency without wearing out Flash.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef BOOT_COUNT_H
#define BOOT_COUNT_H

#include <Arduino.h>

class BootCount {
public:
    BootCount();
    
    void begin();
    void loop();
    bool IsBootCountHigh() const;

private:
};

extern BootCount bootCount;

#endif
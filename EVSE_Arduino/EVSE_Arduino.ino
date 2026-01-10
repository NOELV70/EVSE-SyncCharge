/* =========================================================================================
 * Project:     Evse_Simplified (EVSE-Arduino)
 * Description: A mission-critical, WiFi-enabled Electric Vehicle Supply Equipment (EVSE)
 *              controller built on the dual-core ESP32 platform.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 *
 * CORE CAPABILITIES:
 *   - Smart Protocol Management: Full SAE J1772 implementation with 1kHz PWM generation
 *     and high-precision ADC feedback for vehicle state detection (States A-F).
 *   - IoT & Connectivity: Native MQTT stack with Home Assistant Auto-Discovery,
 *     Captive Portal for zero-preset configuration, and OTA firmware updates.
 *   - Network Intelligence: One-Click transition from DHCP to Static IP with auto-detection.
 *   - Diagnostics: "Cyan-Diag" console showing real-time Pilot Voltage, Heap, and Uptime.
 *
 * SAFETY ARCHITECTURE:
 *   - Hardware Watchdog (WDT): 8-second hardware supervisor to reset MCU on deadlocks.
 *   - Synchronized PWM-Abort: Instantly switches Pilot to +12V (100% duty) on stop/fault
 *     to electronically cease power draw before opening the relay.
 *   - Mechanical Protection: Anti-chatter hysteresis (3000ms) and Pre-Init Pin Lockout
 *     to prevent contactor wear and startup glitches.
 *   - Residual Current Monitor (RCM): Integrated support for RCM fault detection and
 *     periodic self-testing (IEC 62955 / IEC 61851 compliance).
 *
 * HARDWARE CONFIGURATION:
 *   - MCU: ESP32 (Dual Core)
 *   - Relay Control: GPIO 16 (High-Voltage AC Output)
 *   - Pilot PWM: GPIO 27 (1kHz Control Signal)
 *   - Pilot Feedback: GPIO 36 (ADC Input)
 *
 * -----------------------------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * =========================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <Update.h>
#include <esp_task_wdt.h>

#include "EvseLogger.h"
#include "Pilot.h"
#include "EvseCharge.h"
#include "Rcm.h"
#include "EvseMqttController.h"
#include "EvseConfig.h"
#include "WebController.h"

#define BAUD_RATE 115200
#define WDT_TIMEOUT 8 

// Singletons
Pilot pilot;
Rcm rcm;
EvseCharge evse(pilot);
EvseMqttController mqttController(evse);
TaskHandle_t evseTaskHandle = NULL;
AppConfig config;
WebController webController(evse, pilot, mqttController, config);
String deviceId;

/* --- HELPERS --- */

static void applyMqttConfig() {
    if(config.mqttHost.length() > 0) {
        mqttController.begin(config.mqttHost.c_str(), config.mqttPort, config.mqttUser.c_str(), config.mqttPass.c_str(), deviceId);
    }
}


// --- DUAL CORE TASK ---
// Run EVSE logic on a dedicated high-priority task to prevent WiFi/Web lag
// from affecting safety timings.
void evseLoopTask(void* parameter) {
    // SAFETY: Register this task with the Watchdog Timer.
    // Without this, esp_task_wdt_reset() inside this loop does nothing.
    esp_task_wdt_add(NULL);

    for (;;) {
        // SAFETY: Reset watchdog so if main loop blocks, EVSE task prevents hard reboot
        // This ensures charging safety logic continues even if WiFi/Web UI freezes
        esp_task_wdt_reset();
        evse.loop();
        vTaskDelay(pdMS_TO_TICKS(50)); // Run at ~20Hz to prevent starving the Web UI
    }
}

void setup() {
    evse.preinit_hard(); 

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);
    esp_task_wdt_add(NULL); 

    ArduinoOTA.onStart([]() {
        if (evseTaskHandle != NULL) vTaskSuspend(evseTaskHandle);
        evse.stopCharging(); 
        pilot.disable(); 
    });

    Serial.begin(BAUD_RATE);
    uint64_t chipid = ESP.getEfuseMac();   // 48-bit MAC
    char devName[32];

    sprintf(devName, "EVSE-%02X%02X%02X",
            (uint8_t)(chipid >> 24),
            (uint8_t)(chipid >> 32),
            (uint8_t)(chipid >> 40));

    deviceId = String(devName);
    
    loadConfig(config);

    logger.info("================================================");
    logger.infof("  EVSE - KERNEL %s", KERNEL_VERSION);
    logger.infof("  CODENAME : %s", KERNEL_CODENAME);
    logger.infof("  BUILD    : %s %s", __DATE__, __TIME__);
    logger.infof("  DEVICE ID: %s", deviceId.c_str());
    logger.info("================================================");

    ChargingSettings cs;
    cs.maxCurrent = config.maxCurrent; 
    cs.disableAtLowLimit = !config.allowBelow6AmpCharging; // Invert logic for internal struct
    cs.lowLimitResumeDelayMs = config.lowLimitResumeDelayMs;
    evse.setup(cs);
    evse.setRcmEnabled(config.rcmEnabled);

    // RCM Initialization & Self-Test
    rcm.begin();
    if (config.rcmEnabled) {
        if (!rcm.selfTest()) {
            logger.error("[MAIN] Boot-up RCM Self-Test FAILED");
        } else {
            logger.info("[MAIN] Boot-up RCM Self-Test PASSED");
        }
    }

    // Link MQTT Failsafe commands to AppConfig
    mqttController.setFailsafeConfig(config.mqttFailsafeEnabled, config.mqttFailsafeTimeout);
    mqttController.onFailsafeCommand([](bool enabled, unsigned long timeout){
        config.mqttFailsafeEnabled = enabled;
        config.mqttFailsafeTimeout = timeout;
        saveConfig(config); // Persist to NVS
    });
    mqttController.onRcmConfigChanged([](bool enabled){
        config.rcmEnabled = enabled;
        saveConfig(config); // Persist to NVS
    });

    if (config.wifiSsid.length() == 0) {
        webController.begin(deviceId, true);
    } else {
        WiFi.mode(WIFI_STA);
        if (config.useStatic) {
            IPAddress ip, gw, sn;
            if (ip.fromString(config.staticIp) && gw.fromString(config.staticGw) && sn.fromString(config.staticSn)) WiFi.config(ip, gw, sn);
        }
        WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(500); retry++; esp_task_wdt_reset(); }

        if (WiFi.status() != WL_CONNECTED) {
            webController.begin(deviceId, true);
        } else {
            logger.info("[NET] WiFi Connected!");
            logger.infof("[NET] SSID     : %s", config.wifiSsid.c_str());
            logger.infof("[NET] IP ADDR  : %s", WiFi.localIP().toString().c_str());
            logger.infof("[NET] MAC ADDR : %s", WiFi.macAddress().c_str());
            logger.infof("[NET] HOSTNAME : %s", deviceId.c_str());
            applyMqttConfig();
            webController.begin(deviceId, false);
        }
    }
    MDNS.begin(deviceId.c_str());

    ArduinoOTA.begin();

    // Create the Safety Task on Core 1 (App Core) with higher priority (2) than loop (1)
    // This ensures charging logic takes precedence over Network/UI.
    xTaskCreatePinnedToCore(evseLoopTask, "EVSE_Logic", 8192, NULL, 2, &evseTaskHandle, 1);
}

void loop() {
    esp_task_wdt_reset(); 
    webController.loop();
    mqttController.loop();
    ArduinoOTA.handle();
    
    // MQTT HEARTBEAT & FAILSAFE
    static unsigned long lastMqttSeen = 0;
    if (mqttController.connected()) {
        lastMqttSeen = millis();
    } else if (config.mqttFailsafeEnabled && (millis() - lastMqttSeen > (config.mqttFailsafeTimeout * 1000UL))) {
        // If we are charging and haven't seen the broker for [timeout] seconds, STOP.
        if (evse.getState() == STATE_CHARGING) {
            logger.error("[SAFETY] MQTT Connection Lost. Failsafe triggered: Stopping Charge.");
            evse.stopCharging();
        }
    }
}
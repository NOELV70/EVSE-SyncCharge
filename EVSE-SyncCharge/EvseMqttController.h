/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the MQTT controller. Defines topics, MQTT interaction logic,
 *              and Home Assistant discovery integration.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

/*!
 * @file EvseMqttController.h
 * MQTT controller for EVSE (Electric Vehicle Supply Equipment) charger
 * 
 * @copyright (C) Noel Vellemans 2026
 * @license GNU General Public License v2.0 (GPLv2)
 * 
 * @version 1.0.0
 * @date 2026-01-02
 * 
 * @details
 * This module provides MQTT communication and control for the EVSE charger system.
 * It handles both state publishing and command receiving over MQTT protocol.
 * 
 * ## MQTT Commands (Subscribe Topics)
 * 
 * ### 1. Charging Control
 * **Topic:** `evse/{DEVICE_ID}/command`
 * 
 * Commands:
 * - `start`  - Start charging (if vehicle is ready and current limit >= 6A)
 * - `stop`   - Stop charging immediately
 * 
 * Example:
 * ```
 * mosquitto_pub -h 192.168.0.149 -u mqttnoeluser -P mqttpassword \
 *   -t "evse/EVSE-A1B2C3/command" -m "start"
 * ```
 * 
 * ### 2. Current Limit Control
 * **Topic:** `evse/{DEVICE_ID}/setCurrent`
 * 
 * Payload: Floating-point current value in Amperes (6.0 - 32.0 A)
 * 
 * Example:
 * ```
 * mosquitto_pub -h 192.168.0.149 -u mqttnoeluser -P mqttpassword \
 *   -t "evse/EVSE-A1B2C3/setCurrent" -m "16.5"
 * ```
 * 
 * ### 3. PWM/Current Test Mode
 * **Topic:** `evse/{DEVICE_ID}/test/current`
 * 
 * Payloads:
 * - `enable` or `on`    - Enable test mode
 * - `disable` or `off`  - Disable test mode
 * - Numeric (0-100)     - Set PWM duty cycle percentage and enable test mode
 *                         (auto-converts duty % to approximate amps)
 * 
 * Examples:
 * ```
 * # Enable test mode
 * mosquitto_pub -h 192.168.0.149 -u mqttnoeluser -P mqttpassword \
 *   -t "evse/EVSE-A1B2C3/test/current" -m "enable"
 * 
 * # Set test current to 50% PWM (≈ 20A)
 * mosquitto_pub -h 192.168.0.149 -u mqttnoeluser -P mqttpassword \
 *   -t "evse/EVSE-A1B2C3/test/current" -m "50"
 * ```
 * 
 * ## MQTT Status Topics (Publish)
 * 
 * **State Topic:** `evse/{DEVICE_ID}/state`
 * - `0` = STATE_READY (idle, no current)
 * - `1` = STATE_CHARGING (actively charging)
 * 
 * **Vehicle State Topic:** `evse/{DEVICE_ID}/vehicleState`
 * - `0` = VEHICLE_NOT_CONNECTED
 * - `1` = VEHICLE_CONNECTED (connected but not ready)
 * - `2` = VEHICLE_READY (ready to charge)
 * - `3` = VEHICLE_READY_VENTILATION_REQUIRED
 * - `4` = VEHICLE_NO_POWER
 * - `5` = VEHICLE_ERROR
 * 
 * **Current Topic:** `evse/{DEVICE_ID}/current`
 * - Format: `L1,L2,L3` (e.g., "16.50,16.45,16.55")
 * - Current measurements in Amperes for all three phases
 * 
 * **PWM Duty Topic:** `evse/{DEVICE_ID}/pwmDuty`
 * - Pilot signal PWM duty cycle (0-100%)
 * 
 * ## Home Assistant MQTT Discovery
 * 
 * All entities are published with Home Assistant MQTT Discovery enabled
 * under the `homeassistant/*` topics for automatic integration.
 * 
 * @see EvseCharge
 * @see PubSubClient
 */
#ifndef EVSE_MQTT_CONTROLLER_H
#define EVSE_MQTT_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "EvseCharge.h"
#include "Pilot.h"
#include <functional>

class EvseMqttController {
public:
    EvseMqttController(EvseCharge& evseCharge, Pilot& pilotRef);
    void begin(const char* mqttServer, int mqttPort,
               const char* mqttUser, const char* mqttPass,
               const String& deviceIdString, bool useTls = false,
               bool useWs = false, const char* wsUrl = nullptr);
    void loop();
    void enableCurrentTest(bool enable);
    void setFailsafeConfig(bool enabled, unsigned long timeout);
    void onFailsafeCommand(std::function<void(bool, unsigned long)> callback);
    void onRcmConfigChanged(std::function<void(bool)> callback);
    bool connected();
    void incrementWifiConnectCount() { wifiConnectCount++; }
    void incrementMqttConnectCount() { mqttConnectCount++; }

private:
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void publishHADiscovery();
    void publishDiagnosticDiscovery();
    void publishDiagnostics();
    String getRestartReason();
    int getSignalQuality(int rssi);
    
    String serverHost; // Store host to check if configured
    EvseCharge* evse;
    Pilot* pilot;
    WiFiClient mqttWiFiClient;
    WiFiClientSecure mqttWiFiClientSecure;
    PubSubClient mqttClient;
    bool _useTls = false;

    // Exponential backoff for reconnect (resets on success)
    unsigned long _reconnectDelay = 1000;       // Start at 1 second
    static const unsigned long RECONNECT_MIN = 1000;   // 1 second
    static const unsigned long RECONNECT_MAX = 300000; // 5 minutes

    // Failsafe local cache
    bool _fsEnabled = false;
    unsigned long _fsTimeout = 600;
    std::function<void(bool, unsigned long)> _fsCallback;
    std::function<void(bool)> _rcmConfigCallback;

    String deviceId;
    String mqttUser;
    String mqttPass;

    uint32_t wifiConnectCount = 0;
    uint32_t mqttConnectCount = 0;

    // --- Topics ---
    String topicCommand;
    String topicSetCurrent;
    String topicState;
    String topicVehicle;
    String topicCurrent;
    String topicCurrentLimitState;
    String topicPwmDuty;
    String topicSetAllowBelow6AmpCharging;
    // Published state topics for configuration/status
    String topicDisableAtLowLimitState;
    String topicLowLimitResumeDelay;
    // legacy 'setPwm' removed; use topicCurrentTest instead
    String topicCurrentTest;
    String topicSetFailsafe;
    String topicFailsafeState;
    String topicSetFailsafeTimeout;
    String topicFailsafeTimeoutState;
    String topicRcmConfig;      // Command to enable/disable
    String topicRcmState;       // Status of config (1/0)
    String topicRcmFault;       // Fault status (1=Tripped, 0=OK)
    String topicPhaseMode;      // "1-Phase", "3-Phase", "Auto"
    String topicPower;          // Real-time Power (kW)
    String topicAvailability;   // Device availability (online/offline)

    // --- Last values for change detection ---
    STATE_T lastState = STATE_COUNT;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_STATE_COUNT;
    float lastCurrentL1 = -1;
    float lastCurrentL2 = -1;
    float lastCurrentL3 = -1;
    float lastCurrentLimit = -1;
    float lastPwmDuty = -1;
    bool lastRcmTripped = false;
    bool lastRcmEnabled = true;
    int lastPhaseMode = -1;
    float lastPower = -1.0f;
    
    // --- Delayed diagnostics publish (allows HA to process discovery first) ---
    bool _pendingDiagnosticsPublish = false;
    unsigned long _diagnosticsPublishTime = 0;
    
    // --- Timing for reconnect and diagnostics (moved from static locals) ---
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _lastDiagTime = 0;
};

#endif

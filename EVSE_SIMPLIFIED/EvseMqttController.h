/*!
 * @file EvseMqttController.h
 * @brief MQTT controller for EVSE (Electric Vehicle Supply Equipment) charger
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
#include <PubSubClient.h>
#include "EvseCharge.h"
#include "Pilot.h"
#include <functional>

class EvseMqttController {
public:
    EvseMqttController(EvseCharge& evseCharge, Pilot& pilotRef);
    void begin(const char* mqttServer, int mqttPort,
               const char* mqttUser, const char* mqttPass,
               const String& deviceIdString);
    void loop();
    void enableCurrentTest(bool enable);
    void setFailsafeConfig(bool enabled, unsigned long timeout);
    void onFailsafeCommand(std::function<void(bool, unsigned long)> callback);
    void onRcmConfigChanged(std::function<void(bool)> callback);
    bool connected();

private:
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void publishHADiscovery();
    
    String serverHost; // Store host to check if configured
    EvseCharge* evse;
    Pilot* pilot;
    WiFiClient mqttWiFiClient;
    PubSubClient mqttClient;

    // Failsafe local cache
    bool _fsEnabled = false;
    unsigned long _fsTimeout = 600;
    std::function<void(bool, unsigned long)> _fsCallback;
    std::function<void(bool)> _rcmConfigCallback;

    String deviceId;
    String mqttUser;
    String mqttPass;

    // --- Topics ---
    String topicCommand;
    String topicSetCurrent;
    String topicState;
    String topicVehicle;
    String topicCurrent;
    String topicPwmDuty;
    String topicDisableAtLowLimit;
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

    // --- Last values for change detection ---
    STATE_T lastState = STATE_COUNT;
    VEHICLE_STATE_T lastVehicleState = VEHICLE_STATE_COUNT;
    float lastCurrentL1 = -1;
    float lastCurrentL2 = -1;
    float lastCurrentL3 = -1;
    float lastPwmDuty = -1;
    bool lastRcmTripped = false;
    bool lastRcmEnabled = true;
};

#endif

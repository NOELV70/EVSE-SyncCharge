/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the MQTT controller. Handles connection management,
 *              topic subscriptions, command processing, and status publishing.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "EvseMqttController.h"
#include "EvseLogger.h"

EvseMqttController::EvseMqttController(EvseCharge& evseCharge, Pilot& pilotRef)
    : mqttClient(mqttWiFiClient), evse(&evseCharge), pilot(&pilotRef) { }

void EvseMqttController::begin(const char* mqttServer, int mqttPort,
                               const char* mqttUser, const char* mqttPass,
                               const String& deviceIdString)
{
    // Store the server host locally so the loop knows whether to run
    serverHost = mqttServer ? String(mqttServer) : "";
    deviceId = deviceIdString;
    
    // If no host is provided, we stop here. The loop() guard will handle the rest.
    if (serverHost.length() == 0) {
        logger.warn("[MQTT] No host configured. MQTT interface is inactive.");
        return;
    }

    this->mqttUser = mqttUser ? String(mqttUser) : "";
    this->mqttPass = mqttPass ? String(mqttPass) : "";

    // --- Topics ---
    topicCommand                = "evse/" + deviceId + "/command";
    topicSetCurrent             = "evse/" + deviceId + "/setCurrent";
    topicState                  = "evse/" + deviceId + "/state";
    topicVehicle                = "evse/" + deviceId + "/vehicleState";
    topicCurrent                = "evse/" + deviceId + "/current";
    topicPwmDuty                = "evse/" + deviceId + "/pwmDuty";
    topicDisableAtLowLimit      = "evse/" + deviceId + "/setAllowBelow6AmpCharging";
    topicDisableAtLowLimitState = "evse/" + deviceId + "/allowBelow6AmpCharging";
    topicLowLimitResumeDelay    = "evse/" + deviceId + "/lowLimitResumeDelay";
    topicCurrentTest            = "evse/" + deviceId + "/test/current";
    topicSetFailsafe            = "evse/" + deviceId + "/setFailsafe";
    topicFailsafeState          = "evse/" + deviceId + "/failsafe";
    topicSetFailsafeTimeout     = "evse/" + deviceId + "/setFailsafeTimeout";
    topicFailsafeTimeoutState   = "evse/" + deviceId + "/failsafeTimeout";
    topicRcmConfig              = "evse/" + deviceId + "/config/rcm";
    topicRcmState               = "evse/" + deviceId + "/rcm/enabled";
    topicRcmFault               = "evse/" + deviceId + "/rcm/fault";

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        mqttCallback(topic, payload, length);
    });

    logger.infof("[MQTT] Configured for server: %s:%d", mqttServer, mqttPort);
}

void EvseMqttController::loop()
{
    // --- Guard: Self-mute if no host is configured or WiFi is down ---
    // This keeps the main .ino code simple and avoids "ghost" connection attempts.
    if (serverHost.length() == 0 || WiFi.status() != WL_CONNECTED || !evse) {
        return;
    }

    static unsigned long lastAttempt = 0;

    // --- MQTT reconnect logic ---
    if (!mqttClient.connected() && (millis() - lastAttempt > 5000))
    {
        bool connected = false;
        logger.info("[MQTT] Attempting reconnect...");
        
        if (mqttUser.length()) {
            connected = mqttClient.connect(deviceId.c_str(), mqttUser.c_str(), mqttPass.c_str(), topicState.c_str(), 1, true, "offline");
        } else {
            connected = mqttClient.connect(deviceId.c_str(), topicState.c_str(), 1, true, "offline");
        }

        if (connected)
        {
            logger.info("[MQTT] Connected!");

            mqttClient.subscribe(topicCommand.c_str());
            mqttClient.subscribe(topicSetCurrent.c_str());
            mqttClient.subscribe(topicCurrentTest.c_str());
            mqttClient.subscribe(topicDisableAtLowLimit.c_str());
            mqttClient.subscribe(topicSetFailsafe.c_str());
            mqttClient.subscribe(topicSetFailsafeTimeout.c_str());
            mqttClient.subscribe(topicRcmConfig.c_str());

            mqttClient.publish(topicState.c_str(), "online", true);

            // Sync current configuration state to MQTT
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", evse->getAllowBelow6AmpCharging() ? 1 : 0);
            mqttClient.publish(topicDisableAtLowLimitState.c_str(), buf, true);

            snprintf(buf, sizeof(buf), "%lu", evse->getLowLimitResumeDelay());
            mqttClient.publish(topicLowLimitResumeDelay.c_str(), buf, true);

            // Sync Failsafe state
            mqttClient.publish(topicFailsafeState.c_str(), _fsEnabled ? "1" : "0", true);
            snprintf(buf, sizeof(buf), "%lu", _fsTimeout);
            mqttClient.publish(topicFailsafeTimeoutState.c_str(), buf, true);

            // Sync RCM state
            mqttClient.publish(topicRcmState.c_str(), evse->isRcmEnabled() ? "1" : "0", true);
            mqttClient.publish(topicRcmFault.c_str(), evse->isRcmTripped() ? "1" : "0", true);

            publishHADiscovery();
        }
        else
        {
            logger.errorf("[MQTT] Connect failed, rc=%d", mqttClient.state());
        }
        lastAttempt = millis();
    }

    // Run the internal PubSubClient processing
    mqttClient.loop();

    // --- State Reporting (Only on change) ---
    STATE_T s = evse->getState();
    if (s != lastState) {
        char buf[8]; itoa((int)s, buf, 10);
        mqttClient.publish(topicState.c_str(), buf, true);
        lastState = s;
    }

    VEHICLE_STATE_T v = evse->getVehicleState();
    if (v != lastVehicleState) {
        char buf[8]; itoa((int)v, buf, 10);
        mqttClient.publish(topicVehicle.c_str(), buf, true);
        lastVehicleState = v;
    }

    ActualCurrent c = evse->getActualCurrent();
    if (c.l1 != lastCurrentL1 || c.l2 != lastCurrentL2 || c.l3 != lastCurrentL3) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", c.l1, c.l2, c.l3);
        mqttClient.publish(topicCurrent.c_str(), buf, true);
        lastCurrentL1 = c.l1; lastCurrentL2 = c.l2; lastCurrentL3 = c.l3;
    }

    float pwmDuty = evse->getPilotDuty();
    if (pwmDuty != lastPwmDuty) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", pwmDuty);
        mqttClient.publish(topicPwmDuty.c_str(), buf, true);
        lastPwmDuty = pwmDuty;
    }

    bool rcmTripped = evse->isRcmTripped();
    if (rcmTripped != lastRcmTripped) {
        mqttClient.publish(topicRcmFault.c_str(), rcmTripped ? "1" : "0", true);
        lastRcmTripped = rcmTripped;
    }

    bool rcmEn = evse->isRcmEnabled();
    if (rcmEn != lastRcmEnabled) {
        // This handles updates from Web UI reflecting in MQTT
        mqttClient.publish(topicRcmState.c_str(), rcmEn ? "1" : "0", true);
        lastRcmEnabled = rcmEn;
    }
}

void EvseMqttController::mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    logger.infof("[MQTT] Message on %s: %s", topic, msg.c_str());

    if (strcmp(topic, topicCommand.c_str()) == 0)
    {
        if (msg == "start") {
            evse->startCharging();
            evse->signalThrottleAlive();
        }
        else if (msg == "stop")  evse->stopCharging();
    }
    else if (strcmp(topic, topicSetCurrent.c_str()) == 0)
    {
        float amps = msg.toFloat();
        evse->setCurrentLimit(amps);
        evse->signalThrottleAlive();
    }
    else if (strcmp(topic, topicDisableAtLowLimit.c_str()) == 0)
    {
        String lower = msg;
        lower.toLowerCase();
        if (lower == "1" || lower == "on" || lower == "true" || lower == "enable") {
            evse->setAllowBelow6AmpCharging(true);
            // Publish updated state
            mqttClient.publish(topicDisableAtLowLimitState.c_str(), "1", true);
        } else {
            evse->setAllowBelow6AmpCharging(false);
            mqttClient.publish(topicDisableAtLowLimitState.c_str(), "0", true);
        }
    }
    else if (strcmp(topic, topicCurrentTest.c_str()) == 0)
    {
        String lower = msg;
        lower.toLowerCase();

        if (lower == "on" || lower == "enable")
        {
            evse->enableCurrentTest(true);
            mqttClient.publish(topicPwmDuty.c_str(), "current_test_enabled", true);
        }
        else if (lower == "off" || lower == "disable")
        {
            evse->enableCurrentTest(false);
            mqttClient.publish(topicPwmDuty.c_str(), "current_test_disabled", true);
        }
        else
        {
            float duty = msg.toFloat();
            if (duty < 0.0f) duty = 0.0f;
            if (duty > 100.0f) duty = 100.0f;

            // Convert PWM duty (%) to approximate amps using Pilot mapping
            float amps = pilot->dutyToAmps(duty);

            evse->enableCurrentTest(true);
            evse->setCurrentTest(amps);

            char buf[64];
            snprintf(buf, sizeof(buf), "current_test:%.1f%%->%.2fA", duty, amps);
            mqttClient.publish(topicPwmDuty.c_str(), buf, true);
        }
    }
    else if (strcmp(topic, topicSetFailsafe.c_str()) == 0)
    {
        String lower = msg;
        lower.toLowerCase();
        bool newState = (lower == "1" || lower == "on" || lower == "true" || lower == "enable");
        
        if (_fsEnabled != newState) {
            _fsEnabled = newState;
            mqttClient.publish(topicFailsafeState.c_str(), _fsEnabled ? "1" : "0", true);
            if (_fsCallback) _fsCallback(_fsEnabled, _fsTimeout);
        }
    }
    else if (strcmp(topic, topicSetFailsafeTimeout.c_str()) == 0)
    {
        long val = msg.toInt();
        if (val < 10) val = 10; // Minimum 10 seconds safety
        if (val > 3600) val = 3600; // Max 1 hour
        
        if (_fsTimeout != (unsigned long)val) {
            _fsTimeout = (unsigned long)val;
            char buf[16]; snprintf(buf, sizeof(buf), "%lu", _fsTimeout);
            mqttClient.publish(topicFailsafeTimeoutState.c_str(), buf, true);
            if (_fsCallback) _fsCallback(_fsEnabled, _fsTimeout);
        }
    }
    else if (strcmp(topic, topicRcmConfig.c_str()) == 0)
    {
        String lower = msg;
        lower.toLowerCase();
        bool newState = (lower == "1" || lower == "on" || lower == "true" || lower == "enable");
        evse->setRcmEnabled(newState);
        if (_rcmConfigCallback) _rcmConfigCallback(newState);
        // State update handled in loop()
    }

    // legacy topic 'setPwm' removed
}

void EvseMqttController::enableCurrentTest(bool enable)
{
    evse->enableCurrentTest(enable);
}

bool EvseMqttController::connected()
{
    return mqttClient.connected();
}

void EvseMqttController::setFailsafeConfig(bool enabled, unsigned long timeout)
{
    _fsEnabled = enabled;
    _fsTimeout = timeout;
    // If connected, update the state topics immediately
    if (mqttClient.connected()) {
        mqttClient.publish(topicFailsafeState.c_str(), _fsEnabled ? "1" : "0", true);
        char buf[16]; snprintf(buf, sizeof(buf), "%lu", _fsTimeout);
        mqttClient.publish(topicFailsafeTimeoutState.c_str(), buf, true);
    }
}

void EvseMqttController::onFailsafeCommand(std::function<void(bool, unsigned long)> callback) {
    _fsCallback = callback;
}

void EvseMqttController::onRcmConfigChanged(std::function<void(bool)> callback) {
    _rcmConfigCallback = callback;
}

// ---------------------- Home Assistant Discovery ----------------------
void EvseMqttController::publishHADiscovery()
{
    const char* base = "homeassistant";
    char topicBuf[128];
    char payloadBuf[512];

    // --- Switch: Start/Stop charging ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/switch/%s_charging/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE Charging\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"unique_id\":\"%s_charging\",\"device\":{\"identifiers\":[\"%s\"],\"manufacturer\":\"NVL\",\"model\":\"EVSE v1\",\"name\":\"EVSE Charger\"}}",
             topicState.c_str(), topicCommand.c_str(), deviceId.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Sensor: Current ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/sensor/%s_current/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE Current\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"A\",\"unique_id\":\"%s_current\"}",
             topicCurrent.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Sensor: PWM Duty ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/sensor/%s_pwm/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE PWM Duty\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\",\"unique_id\":\"%s_pwm\"}",
             topicPwmDuty.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Sensor: Vehicle State ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/sensor/%s_vehicle/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE Vehicle\",\"state_topic\":\"%s\",\"unique_id\":\"%s_vehicle\"}",
             topicVehicle.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Number: PWM Test for HA slider ---
    // --- Switch: PWM Test enable/disable ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/switch/%s_pwm_test_switch/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE PWM Test Switch\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"payload_on\":\"enable\",\"payload_off\":\"disable\",\"unique_id\":\"%s_pwm_test_switch\"}",
             topicCurrentTest.c_str(), topicPwmDuty.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Number: PWM Test for HA slider (sends percent to test topic) ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/number/%s_pwm_test/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE PWM Test\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\",\"min\":0,\"max\":100,\"step\":1,\"unique_id\":\"%s_pwm_test\"}",
             topicCurrentTest.c_str(), topicPwmDuty.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Switch: Failsafe Enable ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/switch/%s_failsafe/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE MQTT Failsafe\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"payload_on\":\"1\",\"payload_off\":\"0\",\"unique_id\":\"%s_failsafe\"}",
             topicSetFailsafe.c_str(), topicFailsafeState.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Number: Failsafe Timeout ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/number/%s_failsafe_t/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE Failsafe Timeout\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"s\",\"min\":10,\"max\":3600,\"unique_id\":\"%s_failsafe_t\"}",
             topicSetFailsafeTimeout.c_str(), topicFailsafeTimeoutState.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Binary Sensor: RCM Fault ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/binary_sensor/%s_rcm_fault/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE RCM Fault\",\"state_topic\":\"%s\",\"payload_on\":\"1\",\"payload_off\":\"0\",\"device_class\":\"safety\",\"unique_id\":\"%s_rcm_fault\"}",
             topicRcmFault.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    // --- Switch: RCM Enable ---
    snprintf(topicBuf, sizeof(topicBuf), "%s/switch/%s_rcm_enable/config", base, deviceId.c_str());
    snprintf(payloadBuf, sizeof(payloadBuf), "{\"name\":\"EVSE RCM Protection\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"payload_on\":\"1\",\"payload_off\":\"0\",\"unique_id\":\"%s_rcm_enable\"}",
             topicRcmConfig.c_str(), topicRcmState.c_str(), deviceId.c_str());
    mqttClient.publish(topicBuf, payloadBuf, true);

    logger.info("[MQTT] HA discovery published");
}

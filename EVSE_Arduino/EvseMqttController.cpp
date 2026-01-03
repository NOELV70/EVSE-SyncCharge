/*****************************************************************************
 * @file EvseMqttController.cpp
 * @brief MQTT interface for EVSE control and Home Assistant discovery.
 *
 * @details
 * Implements the `EvseMqttController` class which handles MQTT connection,
 * subscriptions for control commands, status publication, and Home Assistant
 * discovery messages.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#include "EvseMqttController.h"
#include "EvseLogger.h"

EvseMqttController::EvseMqttController(EvseCharge& evseCharge)
    : mqttClient(mqttWiFiClient), evse(&evseCharge) { }

void EvseMqttController::begin(const char* mqttServer, int mqttPort,
                               const char* mqttUser, const char* mqttPass,
                               const String& deviceIdString)
{
    // Store the server host locally so the loop knows whether to run
    this->serverHost = mqttServer ? String(mqttServer) : "";
    deviceId = deviceIdString;
    
    // If no host is provided, we stop here. The loop() guard will handle the rest.
    if (this->serverHost.length() == 0) {
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
    topicDisableAtLowLimit      = "evse/" + deviceId + "/setDisableAtLowLimit";
    topicDisableAtLowLimitState = "evse/" + deviceId + "/disableAtLowLimit";
    topicLowLimitResumeDelay    = "evse/" + deviceId + "/lowLimitResumeDelay";
    topicCurrentTest            = "evse/" + deviceId + "/test/current";

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
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

            mqttClient.publish(topicState.c_str(), "online", true);

            // Sync current configuration state to MQTT
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", evse->getDisableAtLowLimit() ? 1 : 0);
            mqttClient.publish(topicDisableAtLowLimitState.c_str(), buf, true);

            snprintf(buf, sizeof(buf), "%lu", evse->getLowLimitResumeDelay());
            mqttClient.publish(topicLowLimitResumeDelay.c_str(), buf, true);

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
}

void EvseMqttController::mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    logger.infof("[MQTT] Message on %s: %s", topic, msg.c_str());

    if (strcmp(topic, topicCommand.c_str()) == 0)
    {
        if (msg == "start") evse->startCharging();
        else if (msg == "stop")  evse->stopCharging();
    }
    else if (strcmp(topic, topicSetCurrent.c_str()) == 0)
    {
        float amps = msg.toFloat();
        evse->setCurrentLimit(amps);
    }
    else if (strcmp(topic, topicDisableAtLowLimit.c_str()) == 0)
    {
        String lower = msg;
        lower.toLowerCase();
        if (lower == "1" || lower == "on" || lower == "true" || lower == "enable") {
            evse->setDisableAtLowLimit(true);
            // Publish updated state
            mqttClient.publish(topicDisableAtLowLimitState.c_str(), "1", true);
        } else {
            evse->setDisableAtLowLimit(false);
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
            float amps = 0.0f;
            const float thresholdDuty = 51.0f / 0.6f; // ~85.0
            if (duty <= thresholdDuty)
                amps = duty * 0.6f;
            else
                amps = (duty - 64.0f) * 2.5f;

            evse->enableCurrentTest(true);
            evse->setCurrentTest(amps);

            char buf[64];
            snprintf(buf, sizeof(buf), "current_test:%.1f%%->%.2fA", duty, amps);
            mqttClient.publish(topicPwmDuty.c_str(), buf, true);
        }
    }
    // legacy topic 'setPwm' removed
}

void EvseMqttController::enableCurrentTest(bool enable)
{
    evse->enableCurrentTest(enable);
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

    logger.info("[MQTT] HA discovery published");
}

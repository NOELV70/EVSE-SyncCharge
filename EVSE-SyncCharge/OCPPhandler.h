/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the OCPPHandler class. Defines the OCPP interface,
 *              connector status structures, and WebSocket client configuration.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#pragma once
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include "EvseTypes.h"

// Single connector; change MAX_CONNECTORS later if needed
#define MAX_CONNECTORS 1

enum ConnectorStatus {
    AVAILABLE,
    CHARGING,
    SUSPENDED,
    UNAVAILABLE
};

struct ConnectorData {
    ConnectorStatus status;
    float currentLimitA;      // from SetChargingProfile
    float measuredCurrentA;   // from ADC/meter
    float measuredVoltageV;
    float measuredPowerW;
    float measuredEnergyWh;
    String errorCode;
};

class EvseCharge;
class Pilot;

class OCPPHandler {
 public:
    OCPPHandler(EvseCharge& evseCharge, Pilot& pilot);
    void begin();
    void setConfig(bool enabled, String host, uint16_t port, String url, bool useTls, String authKey, int heartbeat, int reconnect);
    void loop();

    // Getters for EVSE
    float getCurrentLimit();
    ConnectorStatus getStatus();
    void setConnectorData(float current, float voltage, float power, float energy);

private:
    WebSocketsClient webSocket;
    ConnectorData connector;
    EvseCharge& evse;
    Pilot& pilot;

    bool enabled = false;
    String serverHost;
    uint16_t serverPort = 80;
    String serverUrl = "/";
    bool useTls = false;
    String authKey;
    unsigned long reconnectInterval = 5000;

    unsigned long lastHeartbeat = 0;
    unsigned long heartbeatInterval = 60000; // 60 seconds
    bool connected = false;
    uint32_t messageCounter = 0;  // Incrementing counter for unique message IDs
    String bootNotificationMsgId;

    void wsEvent(WStype_t type, uint8_t* payload, size_t length);
    void onMessage(const String& rawMessage);

    void handleSetChargingProfile(const String& messageId, JsonObject payload);
    void handleRemoteStartTransaction(const String& messageId, JsonObject payload);
    void handleRemoteStopTransaction(const String& messageId, JsonObject payload);

    void sendBootNotification();
    void sendHeartbeat();
    void sendStatusNotification();
    void sendMeterValues();

    // Wrapper for all outgoing CALL messages - handles ID generation and logging
    void sendCall(const char* action, JsonObject& payload);

    String sendAccepted(const String& messageId);
    String sendError(const String& messageId, const char* code, const char* desc);
};

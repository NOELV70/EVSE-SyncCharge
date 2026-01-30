/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the OCPPHandler class. Manages the WebSocket connection
 *              to the OCPP backend, handles JSON serialization/deserialization, and
 *              processes OCPP 1.6J messages.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "OCPPHandler.h"
#include "EvseCharge.h"
#include "Pilot.h"
#include "EvseLogger.h"

OCPPHandler::OCPPHandler(EvseCharge& evseCharge, Pilot& pilotRef) 
    : evse(evseCharge), pilot(pilotRef) 
{
    // Initialize connector defaults
    connector.status = AVAILABLE;
    connector.currentLimitA = 0.0f;
    connector.measuredCurrentA = 0.0f;
    // Default to 230V so power calculations aren't zero if we lack a voltage sensor
    connector.measuredVoltageV = 230.0f; 
    connector.measuredPowerW = 0.0f;
    connector.measuredEnergyWh = 0.0f;
}

void OCPPHandler::setConfig(bool enabled, String host, uint16_t port, String url, bool useTls, String authKey, int heartbeat, int reconnect) {
    this->enabled = enabled;
    this->serverHost = host;
    this->serverPort = port;
    this->serverUrl = url;
    this->useTls = useTls;
    this->authKey = authKey;
    this->heartbeatInterval = heartbeat * 1000UL;
    this->reconnectInterval = reconnect;
    webSocket.setReconnectInterval(this->reconnectInterval);
}

void OCPPHandler::begin() {
    if (!enabled) return;
    
    logger.infof("[OCPP] Connecting to %s:%d%s (%s)", serverHost.c_str(), serverPort, serverUrl.c_str(), useTls ? "WSS" : "WS");
    
    if (useTls) {
        webSocket.beginSSL(serverHost.c_str(), serverPort, serverUrl.c_str(), "", "ocpp1.6");
    } else {
        webSocket.begin(serverHost.c_str(), serverPort, serverUrl.c_str(), "ocpp1.6");
    }
    webSocket.onEvent(std::bind(&OCPPHandler::wsEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    webSocket.setReconnectInterval(reconnectInterval);
}

void OCPPHandler::loop() {
    if (enabled) webSocket.loop();

    if (connected && millis() - lastHeartbeat > heartbeatInterval) {
        sendHeartbeat();
        lastHeartbeat = millis();
    }
}

float OCPPHandler::getCurrentLimit() {
    return evse.getCurrentLimit();
}

ConnectorStatus OCPPHandler::getStatus() {
    STATE_T s = evse.getState();
    if (s == STATE_CHARGING) return CHARGING;

    VEHICLE_STATE_T v = evse.getVehicleState();
    if (v != VEHICLE_NOT_CONNECTED && v != VEHICLE_ERROR && v != VEHICLE_NO_POWER) {
        return SUSPENDED;
    }
    return AVAILABLE;
}

void OCPPHandler::setConnectorData(float current, float voltage, float power, float energy) {
    // These values come FROM the EVSE sensors and will be sent TO the OCPP server
    connector.measuredCurrentA = current;
    connector.measuredVoltageV = voltage;
    connector.measuredPowerW = power;
    connector.measuredEnergyWh = energy;
}

void OCPPHandler::wsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            logger.warn("[OCPP] Disconnected!");
            connected = false;
            break;
        case WStype_CONNECTED:
            logger.info("[OCPP] Connected!");
            connected = true;
            sendBootNotification();
            break;
        case WStype_TEXT:
            onMessage(String((char*)payload));
            break;
        default:
            break;
    }
}

void OCPPHandler::onMessage(const String& rawMessage) {
    logger.debugf("[OCPP] Rx: %s", rawMessage.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, rawMessage);

    if (error) {
        logger.errorf("[OCPP] JSON Error: %s", error.c_str());
        return;
    }

    int msgType = doc[0];

    // OCPP Message Type 2 = CALL
    if (msgType == 2) {
        String messageId = doc[1];
        String action = doc[2];
        JsonObject payload = doc[3];

        if (action == "SetChargingProfile") {
            handleSetChargingProfile(messageId, payload);
        } else if (action == "RemoteStartTransaction") {
            handleRemoteStartTransaction(messageId, payload);
        } else if (action == "RemoteStopTransaction") {
            handleRemoteStopTransaction(messageId, payload);
        } else {
            String err = sendError(messageId, "NotImplemented", "Action not supported");
            webSocket.sendTXT(err);
        }
    } else if (msgType == 3) {
        String id = doc[1];
        if (id == bootNotificationMsgId) {
            JsonObject payload = doc[2];
            if (payload["interval"].is<int>()) {
                int interval = payload["interval"];
                if (interval > 0) {
                    heartbeatInterval = (unsigned long)interval * 1000UL;
                    logger.infof("[OCPP] BootNotification: Heartbeat updated to %ds", interval);
                }
            }
            bootNotificationMsgId = "";
        } else {
            logger.info("[OCPP] Server accepted request");
        }
    } else if (msgType == 4) {
        logger.warnf("[OCPP] Server Error: %s", doc[2].as<const char*>());
    }
}

void OCPPHandler::handleSetChargingProfile(const String& messageId, JsonObject payload) {
    // Simplified parsing for TxDefaultProfile
    if (payload["csChargingProfiles"].is<JsonObject>()) {
        JsonObject cp = payload["csChargingProfiles"];
        if (cp["chargingSchedule"].is<JsonObject>()) {
            JsonObject cs = cp["chargingSchedule"];
            if (cs["chargingSchedulePeriod"].is<JsonArray>()) {
                JsonArray periods = cs["chargingSchedulePeriod"];
                if (periods.size() > 0) {
                    float limit = periods[0]["limit"];
                    connector.currentLimitA = limit;
                    evse.setCurrentLimit(limit);
                    evse.signalThrottleAlive();
                    logger.infof("[OCPP] Set limit to %.1f A", limit);
                }
            }
        }
    }
    String msg = sendAccepted(messageId);
    webSocket.sendTXT(msg);
}

void OCPPHandler::handleRemoteStartTransaction(const String& messageId, JsonObject payload) {
    // In a real scenario, validate idTag here
    evse.startCharging();
    evse.signalThrottleAlive();
    logger.info("[OCPP] Remote Start");
    String msg = sendAccepted(messageId);
    webSocket.sendTXT(msg);
}

void OCPPHandler::handleRemoteStopTransaction(const String& messageId, JsonObject payload) {
    evse.stopCharging();
    logger.info("[OCPP] Remote Stop");
    String msg = sendAccepted(messageId);
    webSocket.sendTXT(msg);
}

void OCPPHandler::sendBootNotification() {
    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["chargePointVendor"] = "EvseSyncCharge";
    payload["chargePointModel"] = "NVL-EVSE";
    sendCall("BootNotification", payload);
}

void OCPPHandler::sendHeartbeat() {
    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    sendCall("Heartbeat", payload);
}

void OCPPHandler::sendStatusNotification() {
    // Placeholder
}

void OCPPHandler::sendMeterValues() {
    // Placeholder
}

void OCPPHandler::sendCall(const char* action, JsonObject& payload) {
    // OCPP CALL: [2, "messageId", "Action", {payload}]
    JsonDocument doc;
    
    if (++messageCounter == 0) ++messageCounter;

    String msgId = String(messageCounter);
    
    if (strcmp(action, "BootNotification") == 0) {
        bootNotificationMsgId = msgId;
    }

    doc.add(2);
    doc.add(msgId);
    doc.add(action);
    doc.add(payload);
    
    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
    logger.debugf("[OCPP] Tx #%s: %s", msgId.c_str(), action);
}

String OCPPHandler::sendAccepted(const String& messageId) {
    // [3, "id", {}]
    JsonDocument doc;
    doc.add(3);
    doc.add(messageId);
    doc.add<JsonObject>();
    String output;
    serializeJson(doc, output);
    return output;
}

String OCPPHandler::sendError(const String& messageId, const char* code, const char* desc) {
    // [4, "id", "code", "desc", {}]
    JsonDocument doc;
    doc.add(4);
    doc.add(messageId);
    doc.add(code);
    doc.add(desc);
    doc.add<JsonObject>();
    String output;
    serializeJson(doc, output);
    return output;
}

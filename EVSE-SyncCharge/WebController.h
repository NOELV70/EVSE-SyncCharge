/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header file for the WebController class. Defines the web server instance,
 *              route handlers, and helper methods for the web interface.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#ifndef WEB_CONTROLLER_H
#define WEB_CONTROLLER_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "EvseCharge.h"
#include "Pilot.h"
#include "EvseMqttController.h"
#include "EvseConfig.h"
#include "OCPPHandler.h"
#include "EvseRfid.h"

class WebController {
public:
    WebController(EvseCharge& evse, Pilot& pilot, EvseMqttController& mqtt, OCPPHandler& ocpp, AppConfig& config, EvseRfid& rfid);
    
    void begin(const String& deviceId, bool apMode);
    void loop();

private:
    WebServer webServer;
    DNSServer dnsServer;
    EvseCharge& evse;
    Pilot& pilot;
    EvseMqttController& mqtt;
    OCPPHandler& ocpp;
    AppConfig& config;
    EvseRfid& rfid;
    
    String deviceId;
    bool apMode;
    bool _rebootPending;
    unsigned long _rebootTimestamp;

    // Helpers
    bool checkAuth();
    String getUptime();
    String getRebootReason();
    String getVehicleStateText();

    // Handlers
    void handleRoot();
    void handleStatus();
    void handleSettingsMenu();
    void handleConfigEvse();
    void handleConfigRcm();
    void handleConfigMqtt();
    void handleConfigWifi();
    void handleConfigOcpp();
    void handleConfigLed();
    void handleConfigTelnet();
    void handleConfigAuth();
    void handleSaveConfig();
    void handleCmd();
    void handleTestMode();
    void handleTestCmd();
    void handleWifiScan();
    void handleFactoryReset();
    void handleWifiReset();
    void handleEvseReset();
    void handleUpdate();
    void handleDoUpdate();
    void handleUpdateUpload();
    void requestReboot();
};

#endif
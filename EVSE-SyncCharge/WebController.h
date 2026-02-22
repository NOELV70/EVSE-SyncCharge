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
#include "Proximity.h"

class WebController {
public:
    WebController(EvseCharge& evse, Pilot& pilot, EvseMqttController& mqtt, OCPPHandler& ocpp, AppConfig& config, EvseRfid& rfid, Proximity& proximity);
    
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
    Proximity& proximity;
    
    String deviceId;
    bool apMode;
    bool _rebootPending;
    unsigned long _rebootTimestamp;
    int _theme;
    String _diagSessionToken;

    // Helpers
    bool checkAuth();
    bool checkHardwareAuth();
    String getUptime();
    String getRebootReason();
    String getVehicleStateText();
    String getDashStyle();
    String getLogoSvg();

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
    void handleHardwareDiagnostics();
    void handleHardwareDiagCmd();
    void handleHardwareStatus();
    void handleHardwareLogin();
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
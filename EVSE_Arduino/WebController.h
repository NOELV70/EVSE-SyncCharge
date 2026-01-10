#ifndef WEB_CONTROLLER_H
#define WEB_CONTROLLER_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "EvseCharge.h"
#include "Pilot.h"
#include "EvseMqttController.h"
#include "EvseConfig.h"

class WebController {
public:
    WebController(EvseCharge& evse, Pilot& pilot, EvseMqttController& mqtt, AppConfig& config);
    
    void begin(const String& deviceId, bool apMode);
    void loop();

private:
    WebServer webServer;
    DNSServer dnsServer;
    EvseCharge& evse;
    Pilot& pilot;
    EvseMqttController& mqtt;
    AppConfig& config;
    
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
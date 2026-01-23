#ifndef EVSE_CONFIG_H
#define EVSE_CONFIG_H

#include <Arduino.h>

/* --- VERSIONING --- */
#define KERNEL_VERSION       "9.0.0"
#define KERNEL_CODENAME      "GOOSE"

struct AppConfig {
    String wifiSsid;
    String wifiPass;
    bool useStatic = false;
    String staticIp = "192.168.1.100";
    String staticGw = "192.168.1.1";
    String staticSn = "255.255.255.0";
    bool mqttEnabled = false;
    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    String wwwUser = "admin";
    String wwwPass = "admin";
    bool allowBelow6AmpCharging = false; // Default false = Strict J1772
    bool pauseImmediate = true;
    unsigned long lowLimitResumeDelayMs = 300000UL;
    float maxCurrent = 32.0f;
    bool mqttFailsafeEnabled = false;
    unsigned long mqttFailsafeTimeout = 600; // Seconds
    bool rcmEnabled = true;
    unsigned long solarStopTimeout = 0; // 0 = Disabled

    // OCPP Configuration
    bool ocppEnabled = false;
    String ocppHost = "";
    uint16_t ocppPort = 80;
    String ocppUrl = "/ocpp/1.6";
    bool ocppUseTls = false;
    String ocppAuthKey = "";
    int ocppHeartbeatInterval = 60;
    int ocppReconnectInterval = 5000;
    int ocppConnTimeout = 10000;
};

// Helper to get version string
inline String getVersionString() {
    char v[64];
    sprintf(v, "Kernel: %s \"%s\"", KERNEL_VERSION, KERNEL_CODENAME);
    return String(v);
}

// Load configuration from NVS (Preferences)
void loadConfig(AppConfig &config);

// Save configuration to NVS (Preferences)
void saveConfig(const AppConfig &config);

#endif
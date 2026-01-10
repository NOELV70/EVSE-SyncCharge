#include "EvseConfig.h"
#include <Preferences.h>

static const char* PREFS_NAMESPACE = "evse_cfg";

void loadConfig(AppConfig &config) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true); // Read-only
    
    config.wifiSsid = prefs.getString("w_ssid", "");
    config.wifiPass = prefs.getString("w_pass", "");
    config.useStatic = prefs.getBool("w_static", false);
    config.staticIp = prefs.getString("w_ip", "192.168.1.100");
    config.staticGw = prefs.getString("w_gw", "192.168.1.1");
    config.staticSn = prefs.getString("w_sn", "255.255.255.0");
    config.mqttHost = prefs.getString("m_host", "");
    config.mqttPort = prefs.getUShort("m_port", 1883);
    config.mqttUser = prefs.getString("m_user", "");
    config.mqttPass = prefs.getString("m_pass", "");
    config.wwwUser  = prefs.getString("w_user", "admin");
    config.wwwPass  = prefs.getString("w_pwd",  "admin");
    config.allowBelow6AmpCharging = prefs.getBool("e_allow_low", false);
    config.pauseImmediate = prefs.getBool("e_pause_im", true);
    config.lowLimitResumeDelayMs = prefs.getULong("e_res_delay", 300000UL);
    config.maxCurrent = prefs.getFloat("e_max_cur", 32.0f);
    config.mqttFailsafeEnabled = prefs.getBool("m_safe", false);
    config.mqttFailsafeTimeout = prefs.getULong("m_safe_t", 600);
    config.rcmEnabled = prefs.getBool("e_rcm_en", true);
    
    prefs.end();
}

void saveConfig(const AppConfig &config) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false); // Read-write
    
    prefs.putString("w_ssid", config.wifiSsid); prefs.putString("w_pass", config.wifiPass);
    prefs.putBool("w_static", config.useStatic);
    prefs.putString("w_ip", config.staticIp); prefs.putString("w_gw", config.staticGw); prefs.putString("w_sn", config.staticSn);
    prefs.putString("m_host", config.mqttHost); prefs.putUShort("m_port", config.mqttPort);
    prefs.putString("m_user", config.mqttUser); prefs.putString("m_pass", config.mqttPass);
    prefs.putString("w_user", config.wwwUser); prefs.putString("w_pwd",  config.wwwPass);
    prefs.putBool("e_allow_low", config.allowBelow6AmpCharging); prefs.putBool("e_pause_im", config.pauseImmediate);
    prefs.putULong("e_res_delay", config.lowLimitResumeDelayMs); prefs.putFloat("e_max_cur", config.maxCurrent);
    prefs.putBool("m_safe", config.mqttFailsafeEnabled);
    prefs.putULong("m_safe_t", config.mqttFailsafeTimeout);
    prefs.putBool("e_rcm_en", config.rcmEnabled);
    
    prefs.end();
}
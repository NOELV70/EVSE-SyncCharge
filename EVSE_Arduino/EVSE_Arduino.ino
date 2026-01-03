/*!
 * @file EVSE_Arduino.ino
 * AUTHOR:      Noel Vellemans
 * VERSION:     5.6.5
 * LICENSE:     GNU General Public License v2.0 (GPLv2)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
*/

/*!
 * @file EVSE_Arduino.ino
 * AUTHOR:      Noel Vellemans
 * VERSION:     5.6.6
 * LICENSE:     GNU General Public License v2.0 (GPLv2)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <Update.h>
#include <esp_task_wdt.h>

#include "EvseLogger.h"
#include "Pilot.h"
#include "EvseCharge.h"
#include "EvseMqttController.h"

/* --- VERSIONING --- */
#define KERNEL_VERSION_MAJOR 5
#define KERNEL_VERSION_MINOR 6
#define KERNEL_VERSION_PATCH 6
#define KERNEL_CODENAME      "Goose-Cyan-Diag"
#define BAUD_RATE 115200
#define WDT_TIMEOUT 8 

// Singletons
Pilot pilot;
EvseCharge evse(pilot);
EvseMqttController mqttController(evse);

struct AppConfig {
    String wifiSsid;
    String wifiPass;
    bool useStatic = false;
    String staticIp = "192.168.1.100";
    String staticGw = "192.168.1.1";
    String staticSn = "255.255.255.0";
    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    String wwwUser = "admin";
    String wwwPass = "admin";
    bool disableAtLowLimit = true;
    bool pauseImmediate = true;
    unsigned long lowLimitResumeDelayMs = 300000UL;
    float maxCurrent = 32.0f;
};

Preferences prefs;
AppConfig config;
static const char* PREFS_NAMESPACE = "evse_cfg";
String deviceId;
static WebServer webServer(80);
static DNSServer dnsServer;
static bool apMode = false;

/* --- UI COMPONENTS --- */
const char* dashStyle =
"<style>"
"* { box-sizing: border-box; }"
"body { background: #121212; color: #eee; font-family: 'Segoe UI', sans-serif; text-align: center; padding: 20px; }"
".container { background: #1e1e1e; border: 2px solid #ffcc00; display: inline-block; padding: 30px; border-radius: 12px; width: 100%; max-width: 560px; box-shadow: 0 10px 40px rgba(0,0,0,0.8); }"
".logo { width: 80px; height: 80px; margin: 0 auto 10px; display: block; fill: #ffcc00; }"
"h1 { color: #ffcc00; margin: 0; letter-spacing: 2px; text-transform: uppercase; font-size: 1.6em; border-bottom: 2px solid #ffcc00; padding-bottom: 5px; }"
".version-tag { color: #ffcc00; font-family: monospace; font-size: 0.85em; margin-bottom: 10px; display: block; letter-spacing: 1px; }"
".stat { background: #2a2a2a; padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid #ffcc00; text-align: left; color: #ffcc00; font-family: monospace; font-size: 0.82em; }"
".diag-header { color: #888; font-size: 0.7em; text-transform: uppercase; text-align: left; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }"
".stat-diag { background: #1a2a2a; padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid #00ffcc; text-align: left; color: #00ffcc; font-family: monospace; font-size: 0.82em; }"
".btn { color: #121212; background: #ffcc00; padding: 12px; border-radius: 6px; font-weight: bold; text-decoration: none; display: inline-block; margin-top: 10px; border: none; cursor: pointer; text-align: center; font-size: 0.95em; width:100%; }"
".btn-red { background: #cc3300; color: #fff; }"
".footer { color: #666; font-size: 0.72em; margin-top: 25px; border-top: 1px solid #333; padding-top: 15px; font-family: monospace; text-align: center; }"
"label { display:block; text-align:left; margin-top:10px; color:#ccc; }"
"input,select { width:100%; padding:10px; border-radius:6px; border:1px solid #333; background:#151515; color:#eee; margin-top:6px; transition: 0.3s; }"
"input:disabled { background: #0f0f0f; color: #444; border-color: #222; opacity: 0.5; cursor: not-allowed; }"
"</style>";

const char* dynamicScript = 
"<script>"
"function toggleStaticFields() {"
"  var isStatic = document.getElementById('mode').value == '1';"
"  var fields = ['ip', 'gw', 'sn'];"
"  fields.forEach(function(f) { document.getElementById(f).disabled = !isStatic; });"
"}"
"window.onload = toggleStaticFields;"
"</script>";

const char* logoSvg = "<svg class='logo' viewBox='0 0 100 100'><path d='M10 50 L50 10 L90 50 V90 H10 Z' fill='none' stroke='#ffcc00' stroke-width='4'/><path d='M30 75 Q30 65 50 65 Q70 65 70 75 L73 82 H27 Z' fill='#ffcc00'/><path d='M45 25 L35 50 H50 L40 75 L65 40 H50 L60 25 Z' fill='#ffcc00' stroke='#121212' stroke-width='1'/></svg>";

/* --- HELPERS --- */
String getVersionString() {
    char v[64];
    sprintf(v, "Kernel: %d.%d.%d \"%s\"", KERNEL_VERSION_MAJOR, KERNEL_VERSION_MINOR, KERNEL_VERSION_PATCH, KERNEL_CODENAME);
    return String(v);
}

String getVehicleStateText() {
    char buffer[50];
    vehicleStateToText((VEHICLE_STATE_T)evse.getState(), buffer);
    return String(buffer);
}

static String getUptime() {
    unsigned long s = millis() / 1000;
    char buf[32];
    sprintf(buf, "%dd %02dh %02dm %02ds", (int)(s/86400), (int)(s%86400)/3600, (int)(s%3600)/60, (int)s%60);
    return String(buf);
}

static String getRebootReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:  return "Power On";
        case ESP_RST_SW:       return "Software Reset";
        case ESP_RST_PANIC:    return "System Crash";
        case ESP_RST_TASK_WDT: return "Task Watchdog";
        case ESP_RST_BROWNOUT: return "Brownout";
        default:               return "Other/Unknown";
    }
}

static bool checkAuth() {
    if (!webServer.authenticate(config.wwwUser.c_str(), config.wwwPass.c_str())) {
        webServer.requestAuthentication(); return false;
    }
    return true;
}

static void loadConfig() {
    prefs.begin(PREFS_NAMESPACE, true);
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
    config.disableAtLowLimit = prefs.getBool("e_dis_low", true);
    config.pauseImmediate = prefs.getBool("e_pause_im", true);
    config.lowLimitResumeDelayMs = prefs.getULong("e_res_delay", 300000UL);
    config.maxCurrent = prefs.getFloat("e_max_cur", 32.0f);
    prefs.end();
}

static void saveConfig() {
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString("w_ssid", config.wifiSsid); prefs.putString("w_pass", config.wifiPass);
    prefs.putBool("w_static", config.useStatic);
    prefs.putString("w_ip", config.staticIp); prefs.putString("w_gw", config.staticGw); prefs.putString("w_sn", config.staticSn);
    prefs.putString("m_host", config.mqttHost); prefs.putUShort("m_port", config.mqttPort);
    prefs.putString("m_user", config.mqttUser); prefs.putString("m_pass", config.mqttPass);
    prefs.putString("w_user", config.wwwUser); prefs.putString("w_pwd",  config.wwwPass);
    prefs.putBool("e_dis_low", config.disableAtLowLimit); prefs.putBool("e_pause_im", config.pauseImmediate);
    prefs.putULong("e_res_delay", config.lowLimitResumeDelayMs); prefs.putFloat("e_max_cur", config.maxCurrent);
    prefs.end();
}

static void applyMqttConfig() {
    if(config.mqttHost.length() > 0) {
        mqttController.begin(config.mqttHost.c_str(), config.mqttPort, config.mqttUser.c_str(), config.mqttPass.c_str(), deviceId);
    }
}

/* --- WEB HANDLERS --- */
static void handleRoot() {
    if (apMode) {
        String host = webServer.hostHeader();
        if (host != WiFi.softAPIP().toString()) {
            webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
            webServer.send(302, "text/plain", "");
            return;
        }
        String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width'>" + String(dashStyle) + "</head><body><div class='container'>";
        h += "<h1>EVSE SETUP</h1><form method='POST' action='/saveConfig'>";
        h += "<label>SSID</label><input name='ssid' id='ssid' value='"+config.wifiSsid+"'>";
        h += "<label>PASS</label><input name='pass' type='password' value='"+config.wifiPass+"'>";
        h += "<label>IP MODE</label><select name='mode' id='mode' onchange='toggleStaticFields()'><option value='0'>DHCP</option><option value='1' "+String(config.useStatic?"selected":"")+">STATIC IP</option></select>";
        h += "<label>IP</label><input name='ip' id='ip' value='"+config.staticIp+"'>";
        h += "<label>GW</label><input name='gw' id='gw' value='"+config.staticGw+"'>";
        h += "<label>SN</label><input name='sn' id='sn' value='"+config.staticSn+"'>";
        h += "<button class='btn' type='submit' style='margin-top:20px;'>SAVE & CONNECT</button></form></div>";
        h += String(dynamicScript) + "</body></html>";
        webServer.send(200, "text/html", h);
        return;
    }

    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>" + String(dashStyle) + "</head><body><div class='container'>" + String(logoSvg);
    h += "<h1>" + deviceId + "</h1><span class='version-tag'>CONTROLLER ONLINE</span>";
    h += "<div class='stat' style='font-size: 1.1em;'>STATUS: " + getVehicleStateText() + "<br>CURRENT LIMIT: " + String(evse.getCurrentLimit(), 1) + " A</div>";
    h += "<div style='display:flex; gap:10px;'><a class='btn' href='/cmd?do=start'>START</a><a class='btn btn-red' href='/cmd?do=stop'>STOP</a></div>";
    h += "<a class='btn' style='margin-top:20px;' href='/settings'>SYSTEM SETTINGS</a>";
    h += "<div class='footer'>SYSTEM: " + getVersionString() + "<br>BUILD: " + String(__DATE__) + " " + String(__TIME__) + "<br>&copy; 2026 Noel Vellemans.</div></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleSettingsMenu() {
    if (!checkAuth()) return;
    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>" + String(dashStyle) + "</head><body><div class='container'><h1>EVSE SETTINGS</h1>";
    h += "<span class='version-tag'>" + getVersionString() + "</span>";
    h += "<div class='stat'><b>IP ADDRESS:</b> " + WiFi.localIP().toString() + "<br><b>WIFI SIGNAL:</b> " + String(WiFi.RSSI()) + " dBm</div>";
    
    // Cyan-Green Diagnostics block
    h += "<div class='diag-header'>System Diagnostics</div>";
    h += "<div class='stat-diag'>";
    h += "<b>PILOT VOLTAGE:</b> " + String(pilot.getVoltage(), 1) + " V<br>";
    h += "<b>FREE HEAP:</b> " + String(ESP.getFreeHeap()) + " Bytes<br>";
    h += "<b>UPTIME:</b> " + getUptime() + "<br>";
    h += "<b>RESET REASON:</b> " + getRebootReason() + "</div>";

    h += "<a href='/config/evse' class='btn'>EVSE PARAMETERS</a><a href='/config/mqtt' class='btn'>MQTT CONFIGURATION</a>";
    h += "<a href='/config/wifi' class='btn'>WIFI & NETWORK</a><a href='/config/auth' class='btn'>ADMIN SECURITY</a>";
    h += "<a href='/update' class='btn' style='background:#004d40; color:#fff;'>FLASH FIRMWARE</a>";
    h += "<a href='/reboot' class='btn btn-red' onclick=\"return confirm('Reboot System?')\">REBOOT DEVICE</a>";
    h += "<a href='/' class='btn' style='background:#444; color:#fff;'>CLOSE</a>";
    h += "</div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleCmd() {
    if (!checkAuth()) return;
    String op = webServer.arg("do");
    if (op == "start") evse.startCharging();
    else if (op == "stop") evse.stopCharging();
    webServer.sendHeader("Location", "/", true); webServer.send(302, "text/plain", "");
}

static void handleSaveConfig() {
    if (!checkAuth() && !apMode) return;
    if (webServer.hasArg("maxcur")) {
        config.maxCurrent = webServer.arg("maxcur").toFloat();
        config.disableAtLowLimit = (webServer.arg("dislow") == "1");
        config.lowLimitResumeDelayMs = webServer.arg("lldelay").toInt();
    }
    if (webServer.hasArg("mqhost")) {
        config.mqttHost = webServer.arg("mqhost"); 
        config.mqttPort = webServer.arg("mqport").toInt();
        config.mqttUser = webServer.arg("mquser"); 
        config.mqttPass = webServer.arg("mqpass");
    }
    if (webServer.hasArg("wuser")) { 
        config.wwwUser = webServer.arg("wuser"); 
        config.wwwPass = webServer.arg("wpass"); 
    }
    if (webServer.hasArg("ssid")) { 
        config.wifiSsid = webServer.arg("ssid"); 
        config.wifiPass = webServer.arg("pass"); 
        if(webServer.hasArg("mode")) {
            config.useStatic = (webServer.arg("mode") == "1");
            config.staticIp = webServer.arg("ip");
            config.staticGw = webServer.arg("gw");
            config.staticSn = webServer.arg("sn");
        }
    }
    saveConfig();
    if (apMode) {
        webServer.send(200, "text/plain", "Rebooting...");
        delay(2000); ESP.restart();
    } else {
        webServer.sendHeader("Location", "/settings", true); webServer.send(302, "text/plain", "");
    }
}

static void handleConfigEvse() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>EVSE Config</h1><form method='POST' action='/saveConfig'>";
    h += "<label>Max Current (A)<input name='maxcur' type='number' step='0.1' value='" + String(config.maxCurrent,1) + "'></label>";
    h += "<label>Behavior<select name='dislow'><option value='1' "+String(config.disableAtLowLimit?"selected":"")+">Stop at 6A</option><option value='0' "+String(!config.disableAtLowLimit?"selected":"")+">Allow < 6A</option></select></label>";
    h += "<label>Resume delay (ms)<input name='lldelay' type='number' value='"+String(config.lowLimitResumeDelayMs)+"'></label>";
    h += "<button class='btn' type='submit'>SAVE</button></form><a class='btn btn-red' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigMqtt() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>MQTT Config</h1><form method='POST' action='/saveConfig'>";
    h += "<label>Host<input name='mqhost' value='"+config.mqttHost+"'></label><label>Port<input name='mqport' type='number' value='"+String(config.mqttPort)+"'></label>";
    h += "<label>User<input name='mquser' value='"+config.mqttUser+"'></label><label>Pass<input name='mqpass' type='password' value='"+config.mqttPass+"'></label>";
    h += "<button class='btn' type='submit'>SAVE</button></form><a class='btn btn-red' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigWifi() {
    if (!checkAuth()) return;

    // Suggest current live values if config is default
    String dispIp = config.staticIp; String dispGw = config.staticGw; String dispSn = config.staticSn;
    if (WiFi.status() == WL_CONNECTED) {
        if (dispIp == "192.168.1.100" || dispIp == "") dispIp = WiFi.localIP().toString();
        if (dispGw == "192.168.1.1"   || dispGw == "") dispGw = WiFi.gatewayIP().toString();
        if (dispSn == "255.255.255.0" || dispSn == "") dispSn = WiFi.subnetMask().toString();
    }

    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>Network Config</h1><form method='POST' action='/saveConfig'>";
    h += "<label>SSID<input name='ssid' id='ssid' value='"+config.wifiSsid+"'></label>";
    h += "<label>Password<input name='pass' type='password' value='"+config.wifiPass+"'></label>";
    h += "<label>IP Assignment<select name='mode' id='mode' onchange='toggleStaticFields()'><option value='0' "+String(!config.useStatic?"selected":"")+">DHCP</option><option value='1' "+String(config.useStatic?"selected":"")+">STATIC IP</option></select></label>";
    h += "<label>Static IP<input name='ip' id='ip' value='"+dispIp+"'></label>";
    h += "<label>Gateway<input name='gw' id='gw' value='"+dispGw+"'></label>";
    h += "<label>Subnet<input name='sn' id='sn' value='"+dispSn+"'></label>";
    h += "<button class='btn' type='submit'>SAVE & RECONNECT</button></form><a class='btn btn-red' href='/settings'>CANCEL</a></div>";
    h += String(dynamicScript) + "</body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigAuth() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>Security</h1><form method='POST' action='/saveConfig'>";
    h += "<label>User<input name='wuser' value='"+config.wwwUser+"'></label><label>Pass<input name='wpass' type='password' value='"+config.wwwPass+"'></label>";
    h += "<button class='btn' type='submit'>SAVE CREDENTIALS</button></form><br>";
    h += "<a href='/factory_reset' class='btn btn-red' onclick=\"return confirm('Wipe ALL settings?')\">FACTORY RESET</a>";
    h += "<a class='btn' style='background:#444;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleFactoryReset() {
    if (!checkAuth()) return;
    webServer.send(200, "text/plain", "Wiping... Rebooting.");
    delay(1000); prefs.begin(PREFS_NAMESPACE, false); prefs.clear(); prefs.end(); ESP.restart();
}

static void handleUpdate() {
    if(!checkAuth()) return;
    String h = "<html><head>"+String(dashStyle)+"</head><body><div class='container'><h1>OTA UPDATE</h1><form method='POST' action='/doUpdate' enctype='multipart/form-data'>";
    h += "<input type='file' name='update' style='margin:20px 0;'><br><input type='submit' value='FLASH' class='btn'></form></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void startCaptivePortal() {
    apMode = true; WiFi.mode(WIFI_AP); WiFi.softAP((deviceId + "-SETUP").c_str());
    dnsServer.start(53, "*", WiFi.softAPIP());
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);
    webServer.onNotFound(handleRoot);
    webServer.begin();
}

void setup() {
    evse.preinit_hard(); 

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);
    esp_task_wdt_add(NULL); 

    ArduinoOTA.onStart([]() {
        evse.stopCharging(); 
        pilot.disable(); 
    });

    Serial.begin(BAUD_RATE);
    uint64_t mac = ESP.getEfuseMac(); 
    deviceId = "EVSE-" + String((uint32_t)(mac >> 32), HEX);
    loadConfig();

    ChargingSettings cs;
    cs.maxCurrent = config.maxCurrent; 
    cs.disableAtLowLimit = config.disableAtLowLimit;
    cs.lowLimitResumeDelayMs = config.lowLimitResumeDelayMs;
    evse.setup(cs);

    if (config.wifiSsid.length() == 0) {
        startCaptivePortal();
    } else {
        WiFi.mode(WIFI_STA);
        if (config.useStatic) {
            IPAddress ip, gw, sn;
            if (ip.fromString(config.staticIp) && gw.fromString(config.staticGw) && sn.fromString(config.staticSn)) WiFi.config(ip, gw, sn);
        }
        WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(500); retry++; esp_task_wdt_reset(); }

        if (WiFi.status() != WL_CONNECTED) {
            startCaptivePortal();
        } else {
            applyMqttConfig();
            webServer.on("/", HTTP_GET, handleRoot);
            webServer.on("/settings", HTTP_GET, handleSettingsMenu);
            webServer.on("/config/evse", HTTP_GET, handleConfigEvse);
            webServer.on("/config/mqtt", HTTP_GET, handleConfigMqtt);
            webServer.on("/config/wifi", HTTP_GET, handleConfigWifi);
            webServer.on("/config/auth", HTTP_GET, handleConfigAuth);
            webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);
            webServer.on("/cmd", HTTP_GET, handleCmd);
            webServer.on("/factory_reset", HTTP_GET, handleFactoryReset);
            webServer.on("/reboot", HTTP_ANY, [](){ if(checkAuth()){ webServer.send(200, "text/plain", "Rebooting..."); delay(1000); ESP.restart(); }});
            webServer.on("/update", HTTP_GET, handleUpdate);
            webServer.on("/doUpdate", HTTP_POST, [](){
                webServer.send(200, "text/plain", (Update.hasError()) ? "Update Failed" : "Success! Rebooting...");
                delay(1000); ESP.restart();
            }, [](){
                HTTPUpload& u = webServer.upload();
                if(u.status == UPLOAD_FILE_START){ if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial); } 
                else if(u.status == UPLOAD_FILE_WRITE){ if(Update.write(u.buf, u.currentSize) != u.currentSize) Update.printError(Serial); } 
                else if(u.status == UPLOAD_FILE_END){ if(!Update.end(true)) Update.printError(Serial); }
            });
            webServer.begin();
        }
    }
    MDNS.begin(deviceId.c_str());
    ArduinoOTA.begin();
}

void loop() {
    esp_task_wdt_reset(); 
    webServer.handleClient();
    if (apMode) dnsServer.processNextRequest();
    evse.loop();
    mqttController.loop();
    ArduinoOTA.handle();
    // MQTT HEARTBEAT & FAILSAFE
    //if (mqttController.connected()) {
    //    lastMqttSeen = millis();
    //} else if (mqttSafetyEnabled && (millis() - lastMqttSeen > MQTT_LOSS_TIMEOUT)) {
    //    if (evse.isCharging()) {
    //        logger.error("[SAFETY] MQTT Connection Lost > 10m. !!! TODO WHAT DO DO HERE !!! ");
    //    }
    //}
}
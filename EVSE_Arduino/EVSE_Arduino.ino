/*
 * =========================================================================================
 * Project:     Evse_Simplified (EVSE-Arduino)
 * Description: A mission-critical, WiFi-enabled Electric Vehicle Supply Equipment (EVSE)
 *              controller built on the dual-core ESP32 platform.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 *
 * CORE CAPABILITIES:
 *   - Smart Protocol Management: Full SAE J1772 implementation with 1kHz PWM generation
 *     and high-precision ADC feedback for vehicle state detection (States A-F).
 *   - IoT & Connectivity: Native MQTT stack with Home Assistant Auto-Discovery,
 *     Captive Portal for zero-preset configuration, and OTA firmware updates.
 *   - Network Intelligence: One-Click transition from DHCP to Static IP with auto-detection.
 *   - Diagnostics: "Cyan-Diag" console showing real-time Pilot Voltage, Heap, and Uptime.
 *
 * SAFETY ARCHITECTURE:
 *   - Hardware Watchdog (WDT): 8-second hardware supervisor to reset MCU on deadlocks.
 *   - Synchronized PWM-Abort: Instantly switches Pilot to +12V (100% duty) on stop/fault
 *     to electronically cease power draw before opening the relay.
 *   - Mechanical Protection: Anti-chatter hysteresis (3000ms) and Pre-Init Pin Lockout
 *     to prevent contactor wear and startup glitches.
 *   - Residual Current Monitor (RCM): Integrated support for RCM fault detection and
 *     periodic self-testing (IEC 62955 / IEC 61851 compliance).
 *
 * HARDWARE CONFIGURATION:
 *   - MCU: ESP32 (Dual Core)
 *   - Relay Control: GPIO 16 (High-Voltage AC Output)
 *   - Pilot PWM: GPIO 27 (1kHz Control Signal)
 *   - Pilot Feedback: GPIO 36 (ADC Input)
 *
 * -----------------------------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * =========================================================================================
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
#include "Rcm.h"
#include "EvseMqttController.h"

/* --- VERSIONING --- */
#define KERNEL_VERSION       "9.0.0"
#define KERNEL_CODENAME      "GOOSE"
#define BAUD_RATE 115200
#define WDT_TIMEOUT 8 

// Singletons
Pilot pilot;
Rcm rcm;
EvseCharge evse(pilot);
EvseMqttController mqttController(evse);
TaskHandle_t evseTaskHandle = NULL;

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
    bool allowBelow6AmpCharging = false; // Default false = Strict J1772
    bool pauseImmediate = true;
    unsigned long lowLimitResumeDelayMs = 300000UL;
    float maxCurrent = 32.0f;
    bool mqttFailsafeEnabled = false;
    unsigned long mqttFailsafeTimeout = 600; // Seconds
    bool rcmEnabled = true;
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
".btn { color: #121212; background: #ffcc00; padding: 12px; border-radius: 6px; font-weight: bold; text-decoration: none; display: inline-block; margin-top: 10px; border: none; cursor: pointer; text-align: center; font-size: 0.95em; width:100%; transition: 0.3s; }"
".btn:hover { opacity: 0.8; }"
".btn-red { background: #cc3300; color: #fff; }"
".footer { color: #666; font-size: 0.95em; margin-top: 25px; border-top: 1px solid #333; padding-top: 15px; font-family: monospace; text-align: center; }"
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

const char* ajaxScript = 
"<script>"
"setInterval(function(){"
"fetch('/status').then(r=>r.json()).then(d=>{"
"document.getElementById('vst').innerText=d.vst;"
"document.getElementById('clim').innerText=d.clim.toFixed(1);"
"document.getElementById('pwm').innerText=d.pwm;"
"document.getElementById('pvolt').innerText=d.pvolt.toFixed(2);"
"document.getElementById('acrel').innerText=d.acrel;"
"document.getElementById('upt').innerText=d.upt;"
"document.getElementById('rssi').innerText=d.rssi;"
"});},1000);"
"</script>";

const char* logoSvg = "<svg class='logo' viewBox='0 0 100 100'><path d='M10 50 L50 10 L90 50 V90 H10 Z' fill='none' stroke='#ffcc00' stroke-width='4'/><path d='M30 75 Q30 65 50 65 Q70 65 70 75 L73 82 H27 Z' fill='#ffcc00'/><path d='M45 25 L35 50 H50 L40 75 L65 40 H50 L60 25 Z' fill='#ffcc00' stroke='#121212' stroke-width='1'/></svg>";

/* --- HELPERS --- */
String getVersionString() {
    char v[64];
    sprintf(v, "Kernel: %s \"%s\"", KERNEL_VERSION, KERNEL_CODENAME);
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
    config.allowBelow6AmpCharging = prefs.getBool("e_allow_low", false);
    config.pauseImmediate = prefs.getBool("e_pause_im", true);
    config.lowLimitResumeDelayMs = prefs.getULong("e_res_delay", 300000UL);
    config.maxCurrent = prefs.getFloat("e_max_cur", 32.0f);
    config.mqttFailsafeEnabled = prefs.getBool("m_safe", false);
    config.mqttFailsafeTimeout = prefs.getULong("m_safe_t", 600);
    config.rcmEnabled = prefs.getBool("e_rcm_en", true);
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
    prefs.putBool("e_allow_low", config.allowBelow6AmpCharging); prefs.putBool("e_pause_im", config.pauseImmediate);
    prefs.putULong("e_res_delay", config.lowLimitResumeDelayMs); prefs.putFloat("e_max_cur", config.maxCurrent);
    prefs.putBool("m_safe", config.mqttFailsafeEnabled);
    prefs.putULong("m_safe_t", config.mqttFailsafeTimeout);
    prefs.putBool("e_rcm_en", config.rcmEnabled);
    prefs.end();
}

static void applyMqttConfig() {
    if(config.mqttHost.length() > 0) {
        mqttController.begin(config.mqttHost.c_str(), config.mqttPort, config.mqttUser.c_str(), config.mqttPass.c_str(), deviceId);
    }
}

static void handleStatus() {
    String json = "{";
    float amps = evse.getCurrentLimit();
    String pwmStr = (evse.getState() == STATE_CHARGING) ? (String(evse.getPilotDuty(), 1) + "%") : "DISABLED";
    json += "\"vst\":\"" + getVehicleStateText() + "\",";
    json += "\"clim\":" + String(amps, 1) + ",";
    json += "\"pwm\":\"" + pwmStr + "\",";
    json += "\"pvolt\":" + String(pilot.getVoltage(), 2) + ",";
    json += "\"acrel\":\"" + String((evse.getState() == STATE_CHARGING) ? "CLOSED" : "OPEN") + "\",";
    json += "\"upt\":\"" + getUptime() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI());
    json += "}";
    webServer.send(200, "application/json", json);
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

        h.reserve(1024); // Prevent heap fragmentation
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
    h.reserve(1500); // Prevent heap fragmentation
    h += "<h1>" + deviceId + "</h1><span class='version-tag'>CONTROLLER ONLINE</span>";
    
    // RED ALERT BANNER FOR RCM FAULT
    if (evse.isRcmEnabled() && evse.isRcmTripped()) {
        h += "<div style='background:#d32f2f; color:#fff; padding:15px; border-radius:6px; margin-bottom:15px; font-weight:bold; border:2px solid #ff5252; animation: blink 1s infinite;'>⚠️ CRITICAL: RCM FAULT DETECTED ⚠️<br><small>Residual Current Monitor Tripped. Disconnect Vehicle to Reset.</small></div><style>@keyframes blink{50%{opacity:0.8}}</style>";
    }

    float amps = evse.getCurrentLimit();
    String pwmStr = (evse.getState() == STATE_CHARGING) ? (String(evse.getPilotDuty(), 1) + "%") : "DISABLED";
    h += "<div class='stat' style='font-size: 1.0em;'>STATUS: <span id='vst'>" + getVehicleStateText() + "</span><br>PWM/CURRENT: <span id='pwm'>" + pwmStr + "</span> (<span id='clim'>" + String(amps, 1) + "</span> A)<br>PILOT VOLTAGE: <span id='pvolt'>" + String(pilot.getVoltage(), 2) + "</span> V<br>AC RELAY: <span id='acrel'>" + String((evse.getState() == STATE_CHARGING) ? "CLOSED" : "OPEN") + "</span></div>";
    h += "<div style='display:flex; gap:10px;'><button class='btn' onclick=\"c('start',this)\">START</button><button class='btn' style='background:#ff9800; color:#fff' onclick=\"c('stop',this)\">PAUSE</button><button class='btn btn-red' onclick=\"c('disable',this)\">STOP</button></div>";
    h += "<script>function c(a,b){var o=b.innerText;b.innerText='...';fetch('/cmd?do='+a+'&ajax=1').then(r=>{setTimeout(()=>{b.innerText=o},500)})}</script>";

    h += "<div class='diag-header'>System Diagnostics</div>";
    h += "<div class='stat-diag' style='font-size: 1.0em;'>";
    h += "<b>UPTIME:</b> <span id='upt'>" + getUptime() + "</span><br>";
    h += "<b>RESET REASON:</b> " + getRebootReason() + "<br>";
    h += "<b>WIFI SIGNAL:</b> <span id='rssi'>" + String(WiFi.RSSI()) + "</span> dBm<br>";
    h += "<b>IP ADDRESS:</b> " + WiFi.localIP().toString() + "</div>";

    h += "<a class='btn' style='margin-top:20px;' href='/settings'>SYSTEM SETTINGS</a>";
    h += "<div class='footer'>SYSTEM: " + getVersionString() + "<br>BUILD: " + String(__DATE__) + " " + String(__TIME__) + "<br>&copy; 2026 Noel Vellemans.</div></div>";
    h += String(ajaxScript) + "</body></html>";
    webServer.send(200, "text/html", h);
}

static void handleSettingsMenu() {
    if (!checkAuth()) return;
    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>" + String(dashStyle) + "</head><body><div class='container'><h1>EVSE SETTINGS</h1>";
    h.reserve(1500); // Prevent heap fragmentation
    h += "<span class='version-tag'>" + getVersionString() + "</span>";
    
    // Cyan-Green Diagnostics block
    h += "<div class='diag-header'>System Diagnostics</div>";
    h += "<div class='stat-diag' style='font-size: 1.0em;'>";
    h += "<b>UPTIME:</b> " + getUptime() + "<br>";
    h += "<b>RESET REASON:</b> " + getRebootReason() + "<br>";
    h += "<b>WIFI SIGNAL:</b> " + String(WiFi.RSSI()) + " dBm<br>";
    h += "<b>IP ADDRESS:</b> " + WiFi.localIP().toString() + "</div>";

    h += "<a href='/config/evse' class='btn'>EVSE PARAMETERS</a><a href='/config/mqtt' class='btn'>MQTT CONFIGURATION</a>";
    h += "<a href='/config/rcm' class='btn'>SYSTEM RCD</a>";
    h += "<a href='/config/wifi' class='btn'>WIFI & NETWORK</a><a href='/config/auth' class='btn'>ADMIN SECURITY</a>";
    h += "<a href='/update' class='btn' style='background:#004d40; color:#fff;'>FLASH FIRMWARE</a>";
    h += "<a href='/reboot' class='btn btn-red' onclick=\"return confirm('Reboot System?')\">REBOOT DEVICE</a>";
    h += "<button class='btn btn-red' style='margin-top:20px' onclick=\"document.getElementById('dz').style.display='block';this.style.display='none'\">⚠️ DANGER ZONE</button>";
    h += "<div id='dz' style='display:none; border:1px solid #cc3300; padding:10px; border-radius:6px; margin-top:10px; background:#2a0a0a'>";
    h += "<form method='POST' action='/factReset' onsubmit=\"return confirm('ERASE ALL?')\"><button class='btn btn-red'>FACTORY RESET</button></form>";
    h += "<div style='display:flex; gap:10px; margin-top:5px;'><form method='POST' action='/wifiReset' style='width:50%' onsubmit=\"return confirm('Reset WiFi Settings?')\"><button class='btn' style='background:#ff9800; color:#fff'>RESET WIFI</button></form>";
    h += "<form method='POST' action='/evseReset' style='width:50%' onsubmit=\"return confirm('Reset EVSE Params?')\"><button class='btn' style='background:#ff9800; color:#fff'>RESET PARAMS</button></form></div></div>";
    h += "<a href='/' class='btn' style='background:#444; color:#fff;'>CLOSE</a>";
    h += "</div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleCmd() {
    if (!checkAuth()) return;
    String op = webServer.arg("do");
    logger.infof("[WEB] Command received: %s", op.c_str());
    if (op == "start") evse.startCharging();
    else if (op == "stop") evse.stopCharging();
    else if (op == "disable") { evse.stopCharging(); pilot.disable(); }
    if (webServer.hasArg("ajax")) 
        webServer.send(200, "text/plain", "OK");
    else 
        { webServer.sendHeader("Location", "/", true); webServer.send(302, "text/plain", ""); }
}

static void handleSaveConfig() {
    if (!checkAuth() && !apMode) return;
    if (webServer.hasArg("maxcur")) {
        float newMax = webServer.arg("maxcur").toFloat();
        // Safety: Constrain current between 6A (J1772 min) and 80A (Reasonable max)
        config.maxCurrent = constrain(newMax, 6.0f, 80.0f);
        config.allowBelow6AmpCharging = (webServer.arg("allowlow") == "1");
        config.lowLimitResumeDelayMs = webServer.arg("lldelay").toInt();
    }
    if (webServer.hasArg("mqhost")) {
        config.mqttHost = webServer.arg("mqhost"); 
        config.mqttPort = webServer.arg("mqport").toInt();
        config.mqttUser = webServer.arg("mquser"); 
        config.mqttPass = webServer.arg("mqpass");
        config.mqttFailsafeEnabled = (webServer.arg("mqsafe") == "1");
        config.mqttFailsafeTimeout = webServer.arg("mqsafet").toInt();
    }
    if (webServer.hasArg("rcmen")) {
        config.rcmEnabled = (webServer.arg("rcmen") == "1");
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
    // Sync changes to MQTT controller so it can publish new state
    mqttController.setFailsafeConfig(config.mqttFailsafeEnabled, config.mqttFailsafeTimeout);
    evse.setRcmEnabled(config.rcmEnabled);

    if (apMode) {
        webServer.send(200, "text/plain", "Rebooting...");
        delay(2000); ESP.restart();
    } else {
        webServer.sendHeader("Location", "/settings", true); webServer.send(302, "text/plain", "");
    }
}

static void handleTestCmd() {
    if (!checkAuth()) return;
    String act = webServer.arg("act");
    if (act == "on") {
        evse.enableCurrentTest(true);
        webServer.send(200, "text/plain", "Enabled");
    } else if (act == "off") {
        evse.enableCurrentTest(false);
        webServer.send(200, "text/plain", "Disabled");
    } else if (act == "pwm") {
        float duty = webServer.arg("val").toFloat();
        // Convert Duty % to Amps (Inverse of J1772)
        float amps = 0.0f;
        if (duty <= 85.0f) amps = duty * 0.6f;
        else amps = (duty - 64.0f) * 2.5f;
        
        // Clamp to configured max current for display consistency
        if (amps > config.maxCurrent) amps = config.maxCurrent;

        evse.setCurrentTest(amps);
        webServer.send(200, "text/plain", String(amps));
    } else {
        webServer.send(400, "text/plain", "Bad Request");
    }
}

static void handleTestMode() {
    if (!checkAuth()) return;
    
    // Calculate max duty cycle based on configured maxCurrent
    int maxDuty = 96;
    if (config.maxCurrent < 80.0f) {
        if (config.maxCurrent <= 51.0f) maxDuty = (int)(config.maxCurrent / 0.6f);
        else maxDuty = (int)((config.maxCurrent / 2.5f) + 64.0f);
    }
    int initVal = (50 > maxDuty) ? maxDuty : 50;

    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>" + String(dashStyle) + "</head><body><div class='container'>";
    h += "<h1>PWM TEST LAB</h1><span class='version-tag'>WARNING: FORCE PWM</span>";
    h += "<div class='stat' style='border-left-color:#673ab7'>PILOT VOLTAGE: <span id='pv'>--</span> V<br>CALC AMPS: <span id='ca'>--</span> A</div>";
    h += "<div style='margin:20px 0; padding:15px; background:#222; border-radius:8px;'>";
    h += "<label>PWM DUTY: <span id='dval'>" + String(initVal) + "</span>%</label>";
    h += "<input type='range' min='10' max='" + String(maxDuty) + "' value='" + String(initVal) + "' style='width:100%' oninput='setPwm(this.value)' onchange='setPwm(this.value)'>";
    h += "</div>";
    h += "<div style='display:flex; gap:10px;'><button class='btn' onclick=\"fetch('/testCmd?act=on')\">ENABLE TEST</button><button class='btn btn-red' onclick=\"fetch('/testCmd?act=off')\">DISABLE TEST</button></div>";
    h += "<a href='/config/evse' class='btn' style='background:#444; margin-top:20px'>BACK</a>";
    h += "<script>";
    h += "function setPwm(v) { document.getElementById('dval').innerText=v; fetch('/testCmd?act=pwm&val='+v).then(r=>r.text()).then(t=>{document.getElementById('ca').innerText=parseFloat(t).toFixed(1);}); }";
    h += "setInterval(function(){ fetch('/status').then(r=>r.json()).then(d=>{ document.getElementById('pv').innerText=d.pvolt.toFixed(2); }); }, 1000);";
    h += "</script></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigEvse() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>EVSE Config</h1><form method='POST' action='/saveConfig'>";
    h += "<label>Max Current (A)<input name='maxcur' type='number' step='0.1' value='" + String(config.maxCurrent,1) + "'></label>";
    h += "<label>Allow Charging < 6A?<select name='allowlow'><option value='0' "+String(!config.allowBelow6AmpCharging?"selected":"")+">No (Strict J1772)</option><option value='1' "+String(config.allowBelow6AmpCharging?"selected":"")+">Yes (Solar/Throttle)</option></select></label>";
    h += "<label>Resume delay (ms)<input name='lldelay' type='number' value='"+String(config.lowLimitResumeDelayMs)+"'></label>";
    h += "<button class='btn' type='submit'>SAVE</button></form>";
    h += "<a href='/test' class='btn' style='background:#673ab7; color:#fff; margin-top:15px;'>PWM TEST LAB</a>";
    h += "<a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigRcm() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>RCD Config</h1><form method='POST' action='/saveConfig'>";
    h += "<div class='stat' style='border-left-color:#ff5252'><b>Residual Current Monitor</b><br>Disabling this safety feature is NOT recommended.</div>";
    h += "<label>RCM Protection<select name='rcmen'><option value='1' "+String(config.rcmEnabled?"selected":"")+">ENABLED (Safe)</option><option value='0' "+String(!config.rcmEnabled?"selected":"")+">DISABLED (Unsafe)</option></select></label>";
    h += "<button class='btn' type='submit'>SAVE</button></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigMqtt() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>MQTT Config</h1><form method='POST' action='/saveConfig'>";
    h += "<label>Host<input name='mqhost' value='"+config.mqttHost+"'></label><label>Port<input name='mqport' type='number' value='"+String(config.mqttPort)+"'></label>";
    h += "<label>User<input name='mquser' value='"+config.mqttUser+"'></label><label>Pass<input name='mqpass' type='password' value='"+config.mqttPass+"'></label>";
    h += "<label>Safety Failsafe<select name='mqsafe'><option value='0' "+String(!config.mqttFailsafeEnabled?"selected":"")+">Disabled</option><option value='1' "+String(config.mqttFailsafeEnabled?"selected":"")+">Stop Charge on Loss</option></select></label>";
    h += "<label>Failsafe Timeout (sec)<input name='mqsafet' type='number' value='"+String(config.mqttFailsafeTimeout)+"'></label>";
    h += "<button class='btn' type='submit'>SAVE</button></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleWifiScan() {
    if (!checkAuth()) return;
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    webServer.send(200, "application/json", json);
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
    h += "<button type='button' class='btn' style='background:#ffcc00' onclick='scanWifi()'>SCAN WIFI</button>";
    h += "<div id='scan-res' style='text-align:left; margin-top:10px; max-height:150px; overflow-y:auto;'></div>";
    h += "<label>Password<input name='pass' type='password' value='"+config.wifiPass+"'></label>";
    h += "<label>IP Assignment<select name='mode' id='mode' onchange='toggleStaticFields()'><option value='0' "+String(!config.useStatic?"selected":"")+">DHCP</option><option value='1' "+String(config.useStatic?"selected":"")+">STATIC IP</option></select></label>";
    h += "<label>Static IP<input name='ip' id='ip' value='"+dispIp+"'></label>";
    h += "<label>Gateway<input name='gw' id='gw' value='"+dispGw+"'></label>";
    h += "<label>Subnet<input name='sn' id='sn' value='"+dispSn+"'></label>";
    h += "<button class='btn' type='submit'>SAVE & RECONNECT</button></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div>";
    h += String(dynamicScript);
    h += "<script>function scanWifi(){document.getElementById('scan-res').innerHTML='Scanning...';fetch('/scan').then(r=>r.json()).then(d=>{var c=document.getElementById('scan-res');c.innerHTML='';d.forEach(n=>{var e=document.createElement('div');e.innerHTML=n.ssid+' <small>('+n.rssi+')</small>';e.style.padding='8px';e.style.borderBottom='1px solid #333';e.style.cursor='pointer';e.onclick=function(){document.getElementById('ssid').value=n.ssid;};c.appendChild(e);});});}</script>";
    h += "</body></html>";
    webServer.send(200, "text/html", h);
}

static void handleConfigAuth() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head>") + dashStyle + "</head><body><div class='container'><h1>Security</h1><form method='POST' action='/saveConfig'>";
    h += "<label>User<input name='wuser' value='"+config.wwwUser+"'></label><label>Pass<input name='wpass' type='password' value='"+config.wwwPass+"'></label>";
    h += "<button class='btn' type='submit'>SAVE CREDENTIALS</button></form><br>";
    h += "<a href='/factory_reset' class='btn btn-red' onclick=\"return confirm('Wipe ALL settings?')\">FACTORY RESET</a>";
    h += "<a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

static void handleFactoryReset() {
    if (!checkAuth()) return;
    // Safety: Stop the logic session and physically disable the pilot signal.
    // Note: stopCharging() alone leaves the pilot active (State B) for normal pauses.
    // For a reset, we want to kill the signal completely.
    evse.stopCharging();
    pilot.disable();

    String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
    h += "<h1>Factory Reset</h1><p>Stopping Charge, Wiping WiFi/Settings, Rebooting...</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
    webServer.send(200, "text/html", h);
    delay(1000); 
    
    // Wipe settings and WiFi credentials
    prefs.begin(PREFS_NAMESPACE, false); prefs.clear(); prefs.end(); 
    WiFi.disconnect(true, true); // Turn off WiFi and erase SDK credentials
    delay(1000);
    ESP.restart();
}

static void handleWifiReset() {
    if (!checkAuth()) return;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.remove("w_ssid");
    prefs.remove("w_pass");
    prefs.remove("w_static");
    prefs.end();
    String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
    h += "<h1>WiFi Reset</h1><p>Credentials cleared. Rebooting into AP Mode...</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
    webServer.send(200, "text/html", h);
    delay(1000);
    WiFi.disconnect(true, true);
    ESP.restart();
}

static void handleEvseReset() {
    if (!checkAuth()) return;
    config.maxCurrent = 32.0f;
    config.rcmEnabled = true;
    config.allowBelow6AmpCharging = false;
    config.lowLimitResumeDelayMs = 300000UL;
    saveConfig();
    
    ChargingSettings cs;
    cs.maxCurrent = config.maxCurrent;
    cs.disableAtLowLimit = !config.allowBelow6AmpCharging;
    cs.lowLimitResumeDelayMs = config.lowLimitResumeDelayMs;
    evse.setup(cs);
    evse.setRcmEnabled(config.rcmEnabled);
    
    webServer.sendHeader("Location", "/settings", true);
    webServer.send(302, "text/plain", "");
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
    logger.info("[NET] Starting Captive Portal (AP Mode)");
    logger.infof("[NET] AP SSID: %s-SETUP", deviceId.c_str());
    logger.infof("[NET] AP IP  : %s", WiFi.softAPIP().toString().c_str());
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);
    webServer.onNotFound(handleRoot);
    webServer.begin();
}

// --- DUAL CORE TASK ---
// Run EVSE logic on a dedicated high-priority task to prevent WiFi/Web lag
// from affecting safety timings.
void evseLoopTask(void* parameter) {
    // SAFETY: Register this task with the Watchdog Timer.
    // Without this, esp_task_wdt_reset() inside this loop does nothing.
    esp_task_wdt_add(NULL);

    for (;;) {
        // SAFETY: Reset watchdog so if main loop blocks, EVSE task prevents hard reboot
        // This ensures charging safety logic continues even if WiFi/Web UI freezes
        esp_task_wdt_reset();
        evse.loop();
        vTaskDelay(pdMS_TO_TICKS(50)); // Run at ~20Hz to prevent starving the Web UI
    }
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
        if (evseTaskHandle != NULL) vTaskSuspend(evseTaskHandle);
        evse.stopCharging(); 
        pilot.disable(); 
    });

    Serial.begin(BAUD_RATE);
    uint64_t chipid = ESP.getEfuseMac();   // 48-bit MAC
    char devName[32];

    sprintf(devName, "EVSE-%02X%02X%02X",
            (uint8_t)(chipid >> 16),
            (uint8_t)(chipid >> 8),
            (uint8_t) chipid);

    deviceId = String(devName);
    
    loadConfig();

    logger.info("================================================");
    logger.infof("  EVSE - KERNEL %s", KERNEL_VERSION);
    logger.infof("  CODENAME : %s", KERNEL_CODENAME);
    logger.infof("  BUILD    : %s %s", __DATE__, __TIME__);
    logger.infof("  DEVICE ID: %s", deviceId.c_str());
    logger.info("================================================");

    ChargingSettings cs;
    cs.maxCurrent = config.maxCurrent; 
    cs.disableAtLowLimit = !config.allowBelow6AmpCharging; // Invert logic for internal struct
    cs.lowLimitResumeDelayMs = config.lowLimitResumeDelayMs;
    evse.setup(cs);
    evse.setRcmEnabled(config.rcmEnabled);

    // RCM Initialization & Self-Test
    rcm.begin();
    if (config.rcmEnabled) {
        rcm.selfTest();
    }

    // Link MQTT Failsafe commands to AppConfig
    mqttController.setFailsafeConfig(config.mqttFailsafeEnabled, config.mqttFailsafeTimeout);
    mqttController.onFailsafeCommand([](bool enabled, unsigned long timeout){
        config.mqttFailsafeEnabled = enabled;
        config.mqttFailsafeTimeout = timeout;
        saveConfig(); // Persist to NVS
    });
    mqttController.onRcmConfigChanged([](bool enabled){
        config.rcmEnabled = enabled;
        saveConfig(); // Persist to NVS
    });

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
            logger.info("[NET] WiFi Connected!");
            logger.infof("[NET] SSID     : %s", config.wifiSsid.c_str());
            logger.infof("[NET] IP ADDR  : %s", WiFi.localIP().toString().c_str());
            logger.infof("[NET] HOSTNAME : %s", deviceId.c_str());
            applyMqttConfig();
            webServer.on("/", HTTP_GET, handleRoot);
            webServer.on("/status", HTTP_GET, handleStatus);
            webServer.on("/settings", HTTP_GET, handleSettingsMenu);
            webServer.on("/config/evse", HTTP_GET, handleConfigEvse);
            webServer.on("/test", HTTP_GET, handleTestMode);
            webServer.on("/config/rcm", HTTP_GET, handleConfigRcm);
            webServer.on("/testCmd", HTTP_GET, handleTestCmd);
            webServer.on("/config/mqtt", HTTP_GET, handleConfigMqtt);
            webServer.on("/config/wifi", HTTP_GET, handleConfigWifi);
            webServer.on("/config/auth", HTTP_GET, handleConfigAuth);
            webServer.on("/scan", HTTP_GET, handleWifiScan);
            webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);
            webServer.on("/cmd", HTTP_GET, handleCmd);
            webServer.on("/factory_reset", HTTP_GET, handleFactoryReset);
            webServer.on("/factReset", HTTP_POST, handleFactoryReset);
            webServer.on("/wifiReset", HTTP_POST, handleWifiReset);
            webServer.on("/evseReset", HTTP_POST, handleEvseReset);
            webServer.on("/reboot", HTTP_ANY, [](){ if(checkAuth()){ webServer.send(200, "text/plain", "Rebooting..."); delay(1000); ESP.restart(); }});
            webServer.on("/update", HTTP_GET, handleUpdate);
            webServer.on("/doUpdate", HTTP_POST, [](){
                String h = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='15;url=/'><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
                if (Update.hasError()) {
                    h += "<h1>Update Failed</h1><p>Please try again.</p><a href='/update' class='btn'>TRY AGAIN</a> <a href='/' class='btn'>HOME</a>";
                } else {
                    h += "<h1>Update Successful!</h1><p>Device is rebooting... You will be redirected shortly.</p><a href='/' class='btn'>RETURN HOME</a>";
                }
                h += "</body></html>";
                webServer.send(200, "text/html", h);
                if (!Update.hasError()) { delay(1000); ESP.restart(); }
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

    // Create the Safety Task on Core 1 (App Core) with higher priority (2) than loop (1)
    // This ensures charging logic takes precedence over Network/UI.
    xTaskCreatePinnedToCore(evseLoopTask, "EVSE_Logic", 8192, NULL, 2, &evseTaskHandle, 1);
}

void loop() {
    esp_task_wdt_reset(); 
    webServer.handleClient();
    if (apMode) dnsServer.processNextRequest();
    mqttController.loop();
    ArduinoOTA.handle();
    
    // MQTT HEARTBEAT & FAILSAFE
    static unsigned long lastMqttSeen = 0;
    if (mqttController.connected()) {
        lastMqttSeen = millis();
    } else if (config.mqttFailsafeEnabled && (millis() - lastMqttSeen > (config.mqttFailsafeTimeout * 1000UL))) {
        // If we are charging and haven't seen the broker for [timeout] seconds, STOP.
        if (evse.getState() == STATE_CHARGING) {
            logger.error("[SAFETY] MQTT Connection Lost. Failsafe triggered: Stopping Charge.");
            evse.stopCharging();
        }
    }
}
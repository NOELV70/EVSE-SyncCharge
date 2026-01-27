/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the WebController class. Handles the embedded web server,
 *              captive portal, and all HTTP API endpoints for configuration and control.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "WebController.h"
#include "WebPages.h"
#include "EvseLogger.h"
#include "OCPPHandler.h"
#include <WiFi.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "RGBWL2812.h"
#include "EvseRfid.h"

WebController::WebController(EvseCharge& evse, Pilot& pilot, EvseMqttController& mqtt, OCPPHandler& ocpp, AppConfig& config)
    : webServer(80), evse(evse), pilot(pilot), mqtt(mqtt), ocpp(ocpp), config(config), apMode(false), _rebootPending(false), _rebootTimestamp(0) {}

extern volatile bool g_otaUpdating;
extern EvseRfid rfid;

void WebController::begin(const String& deviceId, bool apMode) {
    this->deviceId = deviceId;
    this->apMode = apMode;

    if (apMode) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP((deviceId + "-SETUP").c_str());
        dnsServer.start(53, "*", WiFi.softAPIP());
        logger.info("[NET] Starting Captive Portal (AP Mode)");
        logger.infof("[NET] AP SSID: %s-SETUP", deviceId.c_str());
        logger.infof("[NET] AP IP  : %s", WiFi.softAPIP().toString().c_str());
    }

    // Register Routes
    webServer.on("/", HTTP_GET, [this](){ handleRoot(); });
    webServer.on("/status", HTTP_GET, [this](){ handleStatus(); });
    webServer.on("/settings", HTTP_GET, [this](){ handleSettingsMenu(); });
    webServer.on("/config/evse", HTTP_GET, [this](){ handleConfigEvse(); });
    webServer.on("/config/rcm", HTTP_GET, [this](){ handleConfigRcm(); });
    webServer.on("/config/mqtt", HTTP_GET, [this](){ handleConfigMqtt(); });
    webServer.on("/config/wifi", HTTP_GET, [this](){ handleConfigWifi(); });
    webServer.on("/config/ocpp", HTTP_GET, [this](){ handleConfigOcpp(); });
    webServer.on("/config/led", HTTP_GET, [this](){ handleConfigLed(); });
    webServer.on("/config/auth", HTTP_GET, [this](){ handleConfigAuth(); });
    webServer.on("/saveConfig", HTTP_POST, [this](){ handleSaveConfig(); });
    webServer.on("/cmd", HTTP_GET, [this](){ handleCmd(); });
    webServer.on("/test", HTTP_GET, [this](){ handleTestMode(); });
    webServer.on("/testCmd", HTTP_GET, [this](){ handleTestCmd(); });
    webServer.on("/scan", HTTP_GET, [this](){ handleWifiScan(); });
    webServer.on("/factory_reset", HTTP_GET, [this](){ handleFactoryReset(); });
    webServer.on("/factReset", HTTP_POST, [this](){ handleFactoryReset(); });
    webServer.on("/wifiReset", HTTP_POST, [this](){ handleWifiReset(); });
    webServer.on("/evseReset", HTTP_POST, [this](){ handleEvseReset(); });
    webServer.on("/reboot", HTTP_ANY, [this](){ 
        if(checkAuth()){ 
            String h = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='15;url=/'><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
            h += "<h1>Rebooting...</h1><p>System is restarting. You will be redirected shortly.</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
            webServer.send(200, "text/html", h); 
            requestReboot(); 
        }
    });
    webServer.on("/update", HTTP_GET, [this](){ handleUpdate(); });
    webServer.on("/doUpdate", HTTP_POST, [this](){ handleDoUpdate(); }, [this](){ handleUpdateUpload(); });
    
    // --- RFID Routes ---
    webServer.on("/config/rfid", HTTP_GET, [this](){
        if (!checkAuth()) return;
        String h = "<!DOCTYPE html><html><head><title>RFID Config</title>" + String(dashStyle) + "</head><body><div class='container'><h1>RFID Configuration</h1>";
        
        // Enable/Disable Form
        h += "<form id='saveForm' method='POST' action='/rfid/save' style='margin-bottom:15px; padding:15px; background:#222; border-radius:8px;'>";
        h += "<label>RFID Reader Status</label>";
        h += "<select name='en'><option value='1' " + String(rfid.isEnabled()?"selected":"") + ">ENABLED</option><option value='0' " + String(!rfid.isEnabled()?"selected":"") + ">DISABLED</option></select>";
        h += "</form>";

        // Learning Mode
        h += "<div style='margin-bottom:15px; padding:5px; background:#222; border-radius:5px;'><h3>Learning Mode</h3>";
        if(rfid.isLearning()) {
            h += "<script>setTimeout(function(){window.location.reload();}, 800);</script>";
            h += "<p style='color:#ffcc00; animation:blink 1s infinite'>SCAN CARD NOW...</p><a href='/config/rfid' class='btn'>REFRESH</a>";
        } else {
            String last = rfid.getLastScannedUid();
            if(last.length() > 0) h += "<p>Last Scanned: <b>" + last + "</b> <button type='button' class='btn' style='padding:15px; width:auto; font-size:0.8em' onclick=\"document.getElementById('uid').value='" + last + "'\">COPY</button></p>";
            h += "<a href='/rfid/learn' class='btn' style='background:#673ab7; color:#fff'>START LEARNING (10s)</a>";
        }
        h += "</div>";

        // Add Tag Form
        h += "<div style='margin-bottom:20px; padding:15px; background:#222; border-radius:8px;'><h3>Add New Tag</h3>";
        h += "<form method='POST' action='/rfid/add'>";
        h += "<label>UID (Hex)</label><input name='uid' id='uid' placeholder='E.g. A1B2C3D4' required>";
        h += "<label>Tag Name</label><input name='name' placeholder='E.g. Noel Key' required>";
        h += "<button type='submit' class='btn'>ADD TAG</button>";
        h += "</form></div>";

        // Tag List
        h += "<h3>Authorized Tags</h3><div style='overflow-x:auto'><table style='width:100%; border-collapse:collapse; color:#ccc;'>";
        h += "<tr style='background:#333; text-align:left'><th style='padding:10px'>UID</th><th style='padding:10px'>Name</th><th style='padding:10px'>Action</th></tr>";
        std::vector<RfidTag> tags = rfid.getTags();
        for(const auto& t : tags) {
            h += "<tr style='border-bottom:1px solid #444;'>";
            h += "<td style='padding:10px; font-family:monospace'>" + t.uid + "</td>";
            h += "<td style='padding:10px'>" + t.name + "</td>";
            h += "<td style='padding:10px'><form method='POST' action='/rfid/delete' onsubmit=\"return confirm('Delete " + t.name + "?');\"><input type='hidden' name='uid' value='" + t.uid + "'><button type='submit' class='btn btn-red' style='padding:5px 10px; margin:0; width:auto; font-size:0.8em'>DEL</button></form></td>";
            h += "</tr>";
        }
        h += "</table></div>";

        h += "<div style='display:flex; gap:10px; margin-top:20px;'>";
        h += "<button type='button' class='btn' onclick=\"document.getElementById('saveForm').submit();\">SAVE SETTINGS</button>";
        h += "<a class='btn' style='background:#444; color:#fff; margin-top:0;' href='/settings'>BACK</a>";
        h += "</div>";
        h += "<style>@keyframes blink{50%{opacity:0.5}}</style></div></body></html>";
        webServer.send(200, "text/html", h);
    });

    webServer.on("/rfid/save", HTTP_POST, [this](){ if(checkAuth() && webServer.hasArg("en")) rfid.setEnabled(webServer.arg("en")=="1"); webServer.sendHeader("Location", "/settings", true); webServer.send(302, "text/plain", ""); });
    webServer.on("/rfid/add", HTTP_POST, [this](){ if(checkAuth() && webServer.hasArg("uid")) rfid.addTag(webServer.arg("uid"), webServer.arg("name")); webServer.sendHeader("Location", "/config/rfid", true); webServer.send(302, "text/plain", ""); });
    webServer.on("/rfid/delete", HTTP_POST, [this](){ if(checkAuth() && webServer.hasArg("uid")) rfid.deleteTag(webServer.arg("uid")); webServer.sendHeader("Location", "/config/rfid", true); webServer.send(302, "text/plain", ""); });
    webServer.on("/rfid/learn", HTTP_GET, [this](){ if(checkAuth()) rfid.startLearning(); webServer.sendHeader("Location", "/config/rfid", true); webServer.send(302, "text/plain", ""); });

    webServer.onNotFound([this](){ handleRoot(); });
    webServer.begin();
}

void WebController::loop() {
    webServer.handleClient();
    if (apMode) dnsServer.processNextRequest();
    if (_rebootPending && millis() > _rebootTimestamp) {
        ESP.restart();
    }
}

void WebController::requestReboot() {
    _rebootPending = true;
    _rebootTimestamp = millis() + 1000;
}

// --- Helpers ---
bool WebController::checkAuth() {
    if (!webServer.authenticate(config.wwwUser.c_str(), config.wwwPass.c_str())) {
        webServer.requestAuthentication(); return false;
    }
    return true;
}

String WebController::getUptime() {
    unsigned long s = millis() / 1000;
    char buf[32];
    sprintf(buf, "%dd %02dh %02dm %02ds", (int)(s/86400), (int)(s%86400)/3600, (int)(s%3600)/60, (int)s%60);
    return String(buf);
}

String WebController::getRebootReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:  return "Power On";
        case ESP_RST_SW:       return "Software Reset";
        case ESP_RST_PANIC:    return "System Panic";
        case ESP_RST_TASK_WDT: return "Task Watchdog";
        case ESP_RST_BROWNOUT: return "Brownout";
        default:               return "Other/Unknown";
    }
}

String WebController::getVehicleStateText() {
    char buffer[50];
    vehicleStateToText(evse.getVehicleState(), buffer);
    return String(buffer);
}

// --- Handlers ---

void WebController::handleStatus() {
    webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    String json = "{";
    float amps = evse.getCurrentLimit();
    String pwmStr = (evse.getState() == STATE_CHARGING) ? (String(evse.getPilotDuty(), 1) + "%") : "DISABLED";
    json += "\"vst\":\"" + getVehicleStateText() + "\",";
    json += "\"clim\":" + String(amps, 1) + ",";
    json += "\"pwm\":\"" + pwmStr + "\",";
    json += "\"pvolt\":" + String(pilot.getVoltage(), 2) + ",";
    json += "\"acrel\":\"" + String((evse.getState() == STATE_CHARGING) ? "CLOSED" : "OPEN") + "\",";
    json += "\"upt\":\"" + getUptime() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"state\":" + String((int)evse.getState()) + ",";
    json += "\"paused\":" + String(evse.isPaused() ? "true" : "false") + ",";
    json += "\"conn\":" + String(evse.isVehicleConnected() ? "true" : "false");
    json += "}";
    webServer.send(200, "application/json", json);
}

void WebController::handleRoot() {
    if (apMode) {
        String host = webServer.hostHeader();
        if (host != WiFi.softAPIP().toString()) {
            webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
            webServer.send(302, "text/plain", "");
            return;
        }
        String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width'><title>EVSE Setup</title>" + String(dashStyle) + "</head><body><div class='container'>";
        h.reserve(1024);
        h += "<h1>EVSE NETWORK SETUP</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
        h += "<div style='background:#2a2a2a; color:#ffcc00; padding:10px; margin:20px 0 10px 0; border-radius:4px; border-left:4px solid #ffcc00; font-weight:bold;'>WiFi Settings</div>";
        h += "<label>SSID</label><input name='ssid' id='ssid' value='"+config.wifiSsid+"'>";
        h += "<button type='button' class='btn' style='background:#ffcc00' onclick='scanWifi()'>SCAN WIFI</button>";
        h += "<div id='scan-res' style='text-align:left; margin-top:10px; max-height:150px; overflow-y:auto;'></div>";
        h += "<label>PASS</label><input name='pass' type='password' value='"+config.wifiPass+"'>";
        h += "<div style='background:#2a2a2a; color:#ffcc00; padding:10px; margin:20px 0 10px 0; border-radius:4px; border-left:4px solid #ffcc00; font-weight:bold;'>IP Configuration</div>";
        h += "<label>IP MODE</label><select name='mode' id='mode' onchange='toggleStaticFields()'><option value='0'>DHCP</option><option value='1' "+String(config.useStatic?"selected":"")+">STATIC IP</option></select>";
        h += "<label>IP</label><input name='ip' id='ip' value='"+config.staticIp+"'>";
        h += "<label>GW</label><input name='gw' id='gw' value='"+config.staticGw+"'>";
        h += "<label>SN</label><input name='sn' id='sn' value='"+config.staticSn+"'>";
        h += "<button class='btn' type='submit' style='margin-top:20px;'>SAVE & REBOOT</button>";
        h += "<div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form></div>";
        h += String(dynamicScript);
        h += "<script>function scanWifi(){document.getElementById('scan-res').innerHTML='Scanning...';fetch('/scan').then(r=>r.json()).then(d=>{var c=document.getElementById('scan-res');c.innerHTML='';d.forEach(n=>{var e=document.createElement('div');e.innerHTML=n.ssid+' <small>('+n.rssi+')</small>';e.style.padding='8px';e.style.borderBottom='1px solid #333';e.style.cursor='pointer';e.onclick=function(){document.getElementById('ssid').value=n.ssid;Array.from(c.children).forEach(x=>{x.style.background='transparent';x.style.borderLeft='none';});this.style.background='#333';this.style.borderLeft='4px solid #004d40';};c.appendChild(e);});});}</script>";
        h += "</body></html>";
        webServer.send(200, "text/html", h);
        return;
    }

    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>" + deviceId + " - EVSE</title>" + String(dashStyle) + "</head><body><div class='container'>" + String(logoSvg);
    h.reserve(1500);
    h += "<h1>" + deviceId + "</h1><span class='version-tag'>CONTROLLER ONLINE</span>";
    
    if (evse.isRcmEnabled() && evse.isRcmTripped()) {
        h += "<div style='background:#d32f2f; color:#fff; padding:15px; border-radius:6px; margin-bottom:15px; font-weight:bold; border:2px solid #ff5252; animation: blink 1s infinite;'>⚠️ CRITICAL: RCM FAULT DETECTED ⚠️<br><small>Residual Current Monitor Tripped. Disconnect Vehicle to Reset.</small></div><style>@keyframes blink{50%{opacity:0.8}}</style>";
        h += "<div style='background:#d32f2f; color:#fff; padding:15px; border-radius:6px; margin-bottom:15px; font-weight:bold; border:2px solid #ff5252; animation: blink 1s infinite;'>CRITICAL: RCM FAULT DETECTED<br><small>Residual Current Monitor Tripped. Disconnect Vehicle to Reset.</small></div><style>@keyframes blink{50%{opacity:0.8}}</style>";
    }

    float amps = evse.getCurrentLimit();
    String pwmStr = (evse.getState() == STATE_CHARGING) ? (String(evse.getPilotDuty(), 1) + "%") : "DISABLED";
    h += "<div class='stat'><b>VEHICLE STATE:</b> <span id='vst'>" + getVehicleStateText() + "</span></div>";
    h += "<div class='stat'><b>CURRENT LIMIT:</b> <span id='clim'>" + String(amps, 1) + "</span> A<br><b>PWM DUTY:</b> <span id='pwm'>" + pwmStr + "</span></div>";
    h += "<div class='stat'><b>PILOT VOLTAGE:</b> <span id='pvolt'>" + String(pilot.getVoltage(), 2) + "</span> V</div>";
    h += "<div class='stat'><b>AC RELAY:</b> <span id='acrel'>" + String((evse.getState() == STATE_CHARGING) ? "CLOSED" : "OPEN") + "</span></div>";
    
    bool connected = evse.isVehicleConnected();
    
    // START Button: Enabled only if connected AND Idle (not charging/paused)
    bool canStart = connected && (evse.getState() != STATE_CHARGING) && !evse.isPaused();
    String startBtnState = canStart ? "" : " disabled style='cursor:not-allowed; background:#333; color:#777'";

    h += "<div style='display:flex; gap:10px; margin:20px 0;'><button id='btn-start' class='btn'" + startBtnState + " onclick=\"confirmCmd('start', this)\">START CHARGING</button>";
    
    String prStyle = "background:#333; color:#777; cursor:not-allowed"; // Default Disabled
    String prText = "PAUSE CHARGING";
    String prAction = "pause";
    bool prEnabled = false;

    if (evse.getState() == STATE_CHARGING) {
        prStyle = connected ? "background:#ff9800; color:#fff" : "background:#333; color:#777; cursor:not-allowed";
        prEnabled = connected;
    } else if (evse.isPaused()) {
        prStyle = connected ? "background:#4caf50; color:#fff" : "background:#333; color:#777; cursor:not-allowed";
        prText = "RESUME CHARGING";
        prAction = "start";
        prEnabled = connected;
    }
    h += "<button id='btn-pause' class='btn' style='" + prStyle + "' " + (prEnabled ? "" : "disabled") + " onclick=\"confirmCmd('" + prAction + "', this)\">" + prText + "</button>";
    h += "<button id='btn-stop' class='btn btn-red' onclick=\"quickCmd('stop', this)\">STOP CHARGING</button></div>";
    h += "<div id='cm' class='modal'><div class='modal-content'><h2>CONFIRM ACTION</h2><p id='cmsg' style='font-size:1.1em; margin:20px 0; color:#ccc'></p><div style='display:flex; gap:10px'><button id='cyes' class='btn'>YES</button><button onclick=\"document.getElementById('cm').style.display='none'\" class='btn' style='background:#444; color:#fff'>NO</button></div></div></div>";
    h += "<script>function quickCmd(a,b){let o=b.innerText;b.innerText='...';fetch('/cmd?do='+a+'&ajax=1').finally(()=>setTimeout(()=>b.innerText=o,500));} function confirmCmd(a, b) {let m = {'start': 'Resume charging session?','pause': 'Pause charging (vehicle can resume later)?','stop': 'Fully stop charging and disable pilot signal?'}[a]; document.getElementById('cmsg').innerText = m; document.getElementById('cm').style.display = 'block'; document.getElementById('cyes').onclick = function() { document.getElementById('cm').style.display = 'none'; quickCmd(a,b); }; }</script>";

    h += "<div class='diag-header'>System Diagnostics</div>";
    h += "<div class='stat-diag'>";
    h += "<b>UPTIME:</b> <span id='upt'>" + getUptime() + "</span><br>";
    h += "<b>RESET REASON:</b> " + getRebootReason() + "<br>";
    h += "<b>WIFI SIGNAL:</b> <span id='rssi'>" + String(WiFi.RSSI()) + "</span> dBm<br>";
    h += "<b>IP ADDRESS:</b> " + WiFi.localIP().toString() + "</div>";

    h += "<a class='btn' style='margin-top:20px;' href='/settings'>SYSTEM SETTINGS</a>";
    h += "<div class='footer'>SYSTEM: " + getVersionString() + "<br>BUILD: " + String(__DATE__) + " " + String(__TIME__) + "<br>&copy; 2026 Noel Vellemans.</div></div>";
    h += String(ajaxScript) + "</body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleSettingsMenu() {
    if (!checkAuth()) return;
    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>EVSE Settings</title>" + String(dashStyle) + "</head><body><div class='container'><h1>EVSE SETTINGS</h1>";
    h += "<span class='version-tag'>" + getVersionString() + "</span>";
    h += "<div class='diag-header'>System Diagnostics</div>";
    h += "<div class='stat-diag'>";
    h += "<b>UPTIME:</b> " + getUptime() + "<br>";
    h += "<b>RESET REASON:</b> " + getRebootReason() + "<br>";
    h += "<b>WIFI SIGNAL:</b> " + String(WiFi.RSSI()) + " dBm<br>";
    h += "<b>IP ADDRESS:</b> " + WiFi.localIP().toString() + "</div>";
    h += "<div style='margin:20px 0;'>";
    h += "<a href='/config/evse' class='btn'>EVSE PARAMETERS</a>";
    h += "<a href='/config/rcm' class='btn'>RCD SETTINGS</a>";
    h += "<a href='/config/wifi' class='btn'>WIFI & NETWORK</a>";
    h += "<a href='/config/mqtt' class='btn'>MQTT CONFIGURATION</a>";
    h += "<a href='/config/ocpp' class='btn'>OCPP CONFIGURATION</a>";
    h += "<a href='/config/led' class='btn'>LED CONFIGURATION</a>";
    h += "<a href='/config/rfid' class='btn'>RFID MANAGEMENT</a>";
    h += "<a href='/config/auth' class='btn btn-red'>ADMIN SECURITY</a>";
    h += "<a href='/update' class='btn' style='background:#004d40; color:#fff;'>FLASH FIRMWARE</a></div>";
    h += "<a href='/' class='btn' style='background:#444; color:#fff;'>CLOSE</a>";
    h += "</div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigEvse() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head><title>EVSE Config</title>") + dashStyle + "</head><body><div class='container'><h1>EVSE Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<label>Max Current (A)<input name='maxcur' type='number' step='0.1' value='" + String(config.maxCurrent,1) + "'></label>";
    h += "<label>Allow Charging < 6A?<select name='allowlow'><option value='0' "+String(!config.allowBelow6AmpCharging?"selected":"")+">No (Strict J1772)</option><option value='1' "+String(config.allowBelow6AmpCharging?"selected":"")+">Yes (Solar/Throttle)</option></select></label>";
    h += "<label>Resume delay (ms)<input name='lldelay' type='number' value='"+String(config.lowLimitResumeDelayMs)+"'></label>";
    h += "<label>Solar / External Throttle Timeout (sec)<br><small>Throttle to 6A if no update (MQTT/OCPP) (0=Disable)</small><input name='solto' type='number' value='"+String(config.solarStopTimeout)+"'></label>";
    h += "<button class='btn' type='submit'>SAVE</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form>";
    h += "<a href='/test' class='btn' style='background:#673ab7; color:#fff; margin-top:15px;'>PWM TEST LAB</a>";
    h += "<a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigRcm() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head><title>RCD Config</title>") + dashStyle + "</head><body><div class='container'><h1>RCD Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<div class='stat' style='border-left-color:#ff5252'><b>Residual Current Monitor</b><br>Disabling this safety feature is NOT recommended.</div>";
    h += "<div class='stat-diag' style='border-left-color:#ff5252; color:#ff5252'>Changing these settings will trigger a reboot.</div>";
    h += "<label>RCM Protection<select name='rcmen'><option value='1' "+String(config.rcmEnabled?"selected":"")+">ENABLED (Safe)</option><option value='0' "+String(!config.rcmEnabled?"selected":"")+">DISABLED (Unsafe)</option></select></label>";
    h += "<button class='btn' type='submit'>SAVE & REBOOT</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigMqtt() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head><title>MQTT Config</title>") + dashStyle + "</head><body><div class='container'><h1>MQTT Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<div class='stat-diag' style='border-left-color:#ff5252; color:#ff5252'>Changing these settings will trigger a reboot.</div>";
    h += "<label>Enable MQTT<select name='mqen' id='mqen' onchange='toggleMqtt()'><option value='0' "+String(!config.mqttEnabled?"selected":"")+">Disabled</option><option value='1' "+String(config.mqttEnabled?"selected":"")+">Enabled</option></select></label>";
    h += "<div id='mqfields'>";
    h += "<label>Host<input name='mqhost' value='"+config.mqttHost+"'></label><label>Port<input name='mqport' type='number' value='"+String(config.mqttPort)+"'></label>";
    h += "<label>User<input name='mquser' value='"+config.mqttUser+"'></label><label>Pass<input name='mqpass' type='password' value='"+config.mqttPass+"'></label>";
    h += "<label>Safety Failsafe<select name='mqsafe'><option value='0' "+String(!config.mqttFailsafeEnabled?"selected":"")+">Disabled</option><option value='1' "+String(config.mqttFailsafeEnabled?"selected":"")+">Stop Charge on Loss</option></select></label>";
    h += "<label>Failsafe Timeout (sec)<input name='mqsafet' type='number' value='"+String(config.mqttFailsafeTimeout)+"'></label>";
    h += "</div>";
    h += "<button class='btn' type='submit'>SAVE & REBOOT</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a>";
    h += "<script>function toggleMqtt(){var e=document.getElementById('mqen').value=='1';var f=document.getElementById('mqfields');var i=f.getElementsByTagName('input');var s=f.getElementsByTagName('select');for(var k=0;k<i.length;k++)i[k].disabled=!e;for(var k=0;k<s.length;k++)s[k].disabled=!e;f.style.opacity=e?'1':'0.5';}toggleMqtt();</script></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigOcpp() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head><title>OCPP Config</title>") + dashStyle + "</head><body><div class='container'><h1>OCPP Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<div class='stat-diag' style='border-left-color:#ff5252; color:#ff5252'>Changing these settings will trigger a reboot.</div>";
    h += "<label>Enable OCPP<select name='ocppen' id='ocppen' onchange='toggleOcpp()'><option value='0' "+String(!config.ocppEnabled?"selected":"")+">Disabled</option><option value='1' "+String(config.ocppEnabled?"selected":"")+">Enabled</option></select></label>";
    h += "<div id='ofields'>";
    h += "<label>Server Host<input name='ohost' value='"+config.ocppHost+"'></label>";
    h += "<label>Server Port<input name='oport' type='number' value='"+String(config.ocppPort)+"'></label>";
    h += "<label>URL Path (e.g. /ocpp/1.6)<input name='ourl' value='"+config.ocppUrl+"'></label>";
    h += "<label>Use TLS (WSS)<select name='otls'><option value='0' "+String(!config.ocppUseTls?"selected":"")+">No (WS)</option><option value='1' "+String(config.ocppUseTls?"selected":"")+">Yes (WSS)</option></select></label>";
    h += "<label>Auth Key / Tag<input name='okey' value='"+config.ocppAuthKey+"'></label>";
    h += "<label>Heartbeat (sec)<input name='ohb' type='number' value='"+String(config.ocppHeartbeatInterval)+"'></label>";
    h += "<label>Reconnect Interval (ms)<input name='orec' type='number' value='"+String(config.ocppReconnectInterval)+"'></label>";
    h += "<label>Connection Timeout (ms)<input name='oto' type='number' value='"+String(config.ocppConnTimeout)+"'></label>";
    h += "</div>";
    h += "<button class='btn' type='submit'>SAVE & REBOOT</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a>";
    h += "<script>function toggleOcpp(){var e=document.getElementById('ocppen').value=='1';var f=document.getElementById('ofields');var i=f.getElementsByTagName('input');var s=f.getElementsByTagName('select');for(var k=0;k<i.length;k++)i[k].disabled=!e;for(var k=0;k<s.length;k++)s[k].disabled=!e;f.style.opacity=e?'1':'0.5';}toggleOcpp();</script></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigLed() {
    if (!checkAuth()) return;
    LedSettings ls = led.getConfig();
    
    String h = String("<!DOCTYPE html><html><head><title>LED Config</title>") + dashStyle + "</head><body><div class='container'><h1>LED Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    
    h += "<label>Enable LEDs<select name='len' id='len' onchange='toggleLed()'><option value='0' "+String(!ls.enabled?"selected":"")+">Disabled</option><option value='1' "+String(ls.enabled?"selected":"")+">Enabled</option></select></label>";
    h += "<div id='lfields'>";
    h += "<label>Number of LEDs<input name='lnum' type='number' value='"+String(ls.numLeds)+"'></label>";
    
    auto addStateRow = [&](String label, String pfx, LedStateSetting s) {
        h += "<div style='background:#222; padding:10px; margin-top:10px; border-radius:4px;'><b>"+label+"</b><br>";
        h += "<div style='display:flex; gap:10px;'><select name='"+pfx+"_c'>";
        const char* cols[] = {"OFF","RED","GREEN","BLUE","YELLOW","CYAN","MAGENTA","WHITE"};
        for(int i=0;i<8;i++) h += "<option value='"+String(i)+"' "+String(s.color==i?"selected":"")+">"+cols[i]+"</option>";
        h += "</select><select name='"+pfx+"_e'>";
        const char* effs[] = {
            "OFF", "SOLID", "BLINK SLOW", "BLINK FAST", "BREATH", "RAINBOW", 
            "KNIGHT RIDER", "CHASE", "SPARKLE", "THEATER CHASE", "FIRE", "WAVE", 
            "TWINKLE", "COLOR WIPE", "RAINBOW CHASE", "COMET", "PULSE", "STROBE"
        };
        for(int i=0;i<18;i++) h += "<option value='"+String(i)+"' "+String(s.effect==i?"selected":"")+">"+effs[i]+"</option>";
        h += "</select></div></div>";
    };

    addStateRow("Standby (Ready)", "stby", ls.stateStandby);
    addStateRow("Vehicle Connected", "conn", ls.stateConnected);
    addStateRow("Charging", "chg", ls.stateCharging);
    addStateRow("Error / Fault", "err", ls.stateError);
    addStateRow("WiFi Config / AP", "wifi", ls.stateWifi);
    addStateRow("Boot / Startup", "boot", ls.stateBoot);
    addStateRow("Solar Idle (<6A)", "solidle", ls.stateSolarIdle);
    addStateRow("RFID Accepted", "rfidok", ls.stateRfidOk);
    addStateRow("RFID Rejected", "rfidnok", ls.stateRfidReject);

    h += "</div>";
    h += "<button type='button' class='btn' style='background:#673ab7; margin-top:15px; margin-bottom:15px;' onclick=\"fetch('/cmd?do=ledtest&ajax=1')\">TEST LED SEQUENCE (30s)</button>";
    h += "<div style='display:flex; gap:10px;'>";
    h += "<button class='btn' type='submit'>SAVE</button>";
    h += "<a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a>";
    h += "</div><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form>";
    h += "<script>function toggleLed(){var e=document.getElementById('len').value=='1';var f=document.getElementById('lfields');var i=f.getElementsByTagName('input');var s=f.getElementsByTagName('select');for(var k=0;k<i.length;k++)i[k].disabled=!e;for(var k=0;k<s.length;k++)s[k].disabled=!e;f.style.opacity=e?'1':'0.5';}toggleLed();</script></div></body></html>";
    
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigWifi() {
    if (!checkAuth()) return;
    String dispIp = config.staticIp; String dispGw = config.staticGw; String dispSn = config.staticSn;
    if (WiFi.status() == WL_CONNECTED) {
        if (dispIp == "192.168.1.100" || dispIp == "") dispIp = WiFi.localIP().toString();
        if (dispGw == "192.168.1.1"   || dispGw == "") dispGw = WiFi.gatewayIP().toString();
        if (dispSn == "255.255.255.0" || dispSn == "") dispSn = WiFi.subnetMask().toString();
    }
    String h = String("<!DOCTYPE html><html><head><title>Network Config</title>") + dashStyle + "</head><body><div class='container'><h1>Network Config</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<div style='background:#2a2a2a; color:#ffcc00; padding:10px; margin:20px 0 10px 0; border-radius:4px; border-left:4px solid #ffcc00; font-weight:bold;'>WiFi Settings</div>";
    h += "<label>SSID<input name='ssid' id='ssid' value='"+config.wifiSsid+"'></label>";
    h += "<button type='button' class='btn' style='background:#ffcc00' onclick='scanWifi()'>SCAN WIFI</button>";
    h += "<div id='scan-res' style='text-align:left; margin-top:10px; max-height:150px; overflow-y:auto;'></div>";
    h += "<label>Password<input name='pass' type='password' value='"+config.wifiPass+"'></label>";
    h += "<div style='background:#2a2a2a; color:#ffcc00; padding:10px; margin:20px 0 10px 0; border-radius:4px; border-left:4px solid #ffcc00; font-weight:bold;'>IP Configuration</div>";
    h += "<label>IP Assignment<select name='mode' id='mode' onchange='toggleStaticFields()'><option value='0' "+String(!config.useStatic?"selected":"")+">DHCP</option><option value='1' "+String(config.useStatic?"selected":"")+">STATIC IP</option></select></label>";
    h += "<label>Static IP<input name='ip' id='ip' value='"+dispIp+"'></label>";
    h += "<label>Gateway<input name='gw' id='gw' value='"+dispGw+"'></label>";
    h += "<label>Subnet<input name='sn' id='sn' value='"+dispSn+"'></label>";
    h += "<button class='btn' type='submit'>SAVE & REBOOT</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form><a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div>";
    h += String(dynamicScript);
    h += "<script>function scanWifi(){document.getElementById('scan-res').innerHTML='Scanning...';fetch('/scan').then(r=>r.json()).then(d=>{var c=document.getElementById('scan-res');c.innerHTML='';d.forEach(n=>{var e=document.createElement('div');e.innerHTML=n.ssid+' <small>('+n.rssi+')</small>';e.style.padding='8px';e.style.borderBottom='1px solid #333';e.style.cursor='pointer';e.onclick=function(){document.getElementById('ssid').value=n.ssid;Array.from(c.children).forEach(x=>{x.style.background='transparent';x.style.borderLeft='none';});this.style.background='#333';this.style.borderLeft='4px solid #004d40';};c.appendChild(e);});});}</script>";
    h += "</body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleConfigAuth() {
    if (!checkAuth()) return;
    String h = String("<!DOCTYPE html><html><head><title>Security Config</title>") + dashStyle + "</head><body><div class='container'><h1>Security</h1><form method='POST' action='/saveConfig' onsubmit=\"document.getElementById('saveMsg').style.display='block'; document.getElementById('saveMsg').innerText='Saving...';\">";
    h += "<label>User<input name='wuser' value='"+config.wwwUser+"'></label><label>Pass<input name='wpass' type='password' value='"+config.wwwPass+"'></label>";
    h += "<button class='btn' type='submit'>SAVE CREDENTIALS</button><div id='saveMsg' style='margin-top:10px; display:none; color:#00ffcc; font-weight:bold;'></div></form><br>";
    h += "<button class='btn btn-red' onclick=\"cfm('Reboot System?', function(){window.location='/reboot'})\">REBOOT DEVICE</button>";
    h += "<button class='btn btn-red' style='margin-top:20px' onclick=\"document.getElementById('dz').style.display='block';this.style.display='none'\">! DANGER ZONE !</button>";
    h += "<div id='dz' style='display:none; border:1px solid #cc3300; padding:10px; border-radius:6px; margin-top:10px; background:#2a0a0a'>";
    h += "<form id='f1' method='POST' action='/factReset'><button type='button' class='btn btn-red' onclick=\"cfm('ERASE ALL DATA?', function(){document.getElementById('f1').submit()})\">FACTORY RESET</button></form>";
    h += "<div style='display:flex; gap:10px; margin-top:5px;'><form id='f2' method='POST' action='/wifiReset' style='width:50%'><button type='button' class='btn' style='background:#ff9800; color:#fff' onclick=\"cfm('Reset WiFi Settings?', function(){document.getElementById('f2').submit()})\">RESET WIFI</button></form>";
    h += "<form id='f3' method='POST' action='/evseReset' style='width:50%'><button type='button' class='btn' style='background:#ff9800; color:#fff' onclick=\"cfm('Reset EVSE Params?', function(){document.getElementById('f3').submit()})\">RESET PARAMS</button></form></div></div>";
    h += "<a class='btn' style='background:#444; color:#fff; margin-top:20px;' href='/settings'>CANCEL</a>";
    h += "<div id='cm' class='modal'><div class='modal-content'><h2>CONFIRM ACTION</h2><p id='cmsg' style='font-size:1.1em; margin:20px 0; color:#ccc'></p><div style='display:flex; gap:10px'><button id='cyes' class='btn'>YES</button><button onclick=\"document.getElementById('cm').style.display='none'\" class='btn' style='background:#444; color:#fff'>NO</button></div></div></div>";
    h += "<script>var pa=null;function cfm(m,a){document.getElementById('cmsg').innerText=m;document.getElementById('cm').style.display='block';pa=a;}document.getElementById('cyes').onclick=function(){document.getElementById('cm').style.display='none';if(pa)pa();};</script>";
    h += "</div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleSaveConfig() {
    if (!checkAuth() && !apMode) return;
    bool rebootRequired = false;

    if (webServer.hasArg("maxcur")) {
        float newMax = webServer.arg("maxcur").toFloat();
        config.maxCurrent = constrain(newMax, 6.0f, 80.0f);
        config.allowBelow6AmpCharging = (webServer.arg("allowlow") == "1");
        config.lowLimitResumeDelayMs = webServer.arg("lldelay").toInt();
        config.solarStopTimeout = webServer.arg("solto").toInt();
    }
    if (webServer.hasArg("mqhost")) {
        rebootRequired = true;
        config.mqttEnabled = (webServer.arg("mqen") == "1");
        config.mqttHost = webServer.arg("mqhost"); 
        config.mqttPort = webServer.arg("mqport").toInt();
        config.mqttUser = webServer.arg("mquser"); 
        config.mqttPass = webServer.arg("mqpass");
        config.mqttFailsafeEnabled = (webServer.arg("mqsafe") == "1");
        config.mqttFailsafeTimeout = webServer.arg("mqsafet").toInt();
    }
    if (webServer.hasArg("ohost")) {
        rebootRequired = true;
        config.ocppEnabled = (webServer.arg("ocppen") == "1");
        config.ocppHost = webServer.arg("ohost");
        config.ocppPort = webServer.arg("oport").toInt();
        config.ocppUrl = webServer.arg("ourl");
        config.ocppUseTls = (webServer.arg("otls") == "1");
        config.ocppAuthKey = webServer.arg("okey");
        config.ocppHeartbeatInterval = webServer.arg("ohb").toInt();
        config.ocppReconnectInterval = webServer.arg("orec").toInt();
        config.ocppConnTimeout = webServer.arg("oto").toInt();
    }
    if (webServer.hasArg("len")) {
        LedSettings ls;
        ls.enabled = (webServer.arg("len") == "1");
        ls.numLeds = webServer.arg("lnum").toInt();
        
        auto getSt = [&](String pfx) -> LedStateSetting {
            return {(LedColor)webServer.arg(pfx+"_c").toInt(), (LedEffect)webServer.arg(pfx+"_e").toInt()};
        };
        ls.stateStandby = getSt("stby");
        ls.stateConnected = getSt("conn");
        ls.stateCharging = getSt("chg");
        ls.stateError = getSt("err");
        ls.stateWifi = getSt("wifi");
        ls.stateBoot = getSt("boot");
        ls.stateSolarIdle = getSt("solidle");
        ls.stateRfidOk = getSt("rfidok");
        ls.stateRfidReject = getSt("rfidnok");
        
        led.updateConfig(ls);
    }
    if (webServer.hasArg("rcmen")) {
        config.rcmEnabled = (webServer.arg("rcmen") == "1");
        rebootRequired = true;
    }
    if (webServer.hasArg("wuser")) { config.wwwUser = webServer.arg("wuser"); config.wwwPass = webServer.arg("wpass"); }
    if (webServer.hasArg("ssid")) { 
        rebootRequired = true;
        config.wifiSsid = webServer.arg("ssid"); config.wifiPass = webServer.arg("pass"); 
        if(webServer.hasArg("mode")) {
            config.useStatic = (webServer.arg("mode") == "1");
            config.staticIp = webServer.arg("ip"); config.staticGw = webServer.arg("gw"); config.staticSn = webServer.arg("sn");
        }
    }
    saveConfig(config);
    mqtt.setFailsafeConfig(config.mqttFailsafeEnabled, config.mqttFailsafeTimeout);
    evse.setThrottleAliveTimeout(config.solarStopTimeout);
    ocpp.setConfig(config.ocppEnabled, config.ocppHost, config.ocppPort, config.ocppUrl, config.ocppUseTls, config.ocppAuthKey, config.ocppHeartbeatInterval, config.ocppReconnectInterval);
    evse.setRcmEnabled(config.rcmEnabled);

    if (apMode || rebootRequired) {
        String h = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='15;url=/'><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
        h += "<h1>Settings Saved</h1><p>System is rebooting to apply changes...</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
        webServer.send(200, "text/html", h);
        requestReboot();
    } else {
        webServer.sendHeader("Location", "/settings", true); webServer.send(302, "text/plain", "");
    }
}

void WebController::handleCmd() {
    if (!checkAuth()) return;
    String op = webServer.arg("do");
    logger.infof("[WEB] Command received: %s", op.c_str());
    if (op == "start") evse.startCharging();
    else if (op == "pause") evse.pauseCharging();
    else if (op == "stop") { evse.stopCharging(); pilot.disable(); }
    else if (op == "ledtest") { led.startTestSequence(); }
    if (webServer.hasArg("ajax")) webServer.send(200, "text/plain", "OK");
    else { webServer.sendHeader("Location", "/", true); webServer.send(302, "text/plain", ""); }
}

void WebController::handleTestMode() {
    if (!checkAuth()) return;
    int maxDuty = (int)pilot.ampsToDuty(MAX_CURRENT);
    int initVal = 50;
    String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>PWM Test Lab</title>" + String(dashStyle) + "</head><body><div class='container'>";
    h += "<h1>PWM TEST LAB</h1><span class='version-tag'>WARNING: FORCE PWM</span>";
    h += "<div class='stat' style='border-left-color:#673ab7'>PILOT VOLTAGE: <span id='pv'>--</span> V<br>CALC AMPS: <span id='ca'>--</span> A</div>";
    h += "<div style='margin:20px 0; padding:15px; background:#222; border-radius:8px;'>";
    h += "<label>PWM DUTY: <span id='dval'>" + String(initVal) + "</span>%</label>";
    h += "<input type='range' min='10' max='" + String(maxDuty) + "' value='" + String(initVal) + "' style='width:100%' oninput='setPwm(this.value)' onchange='setPwm(this.value)'>";
    h += "</div>";
    h += "<div style='display:flex; gap:10px;'><button class='btn' onclick=\"fetch('/testCmd?act=on')\">ENABLE TEST</button><button class='btn btn-red' onclick=\"fetch('/testCmd?act=off')\">DISABLE TEST</button></div>";
    h += "<a href='/config/evse' class='btn' style='background:#444; margin-top:20px'>BACK</a>";
    h += "<script>function setPwm(v) { document.getElementById('dval').innerText=v; fetch('/testCmd?act=pwm&val='+v).then(r=>r.text()).then(t=>{document.getElementById('ca').innerText=parseFloat(t).toFixed(1);}); } setInterval(function(){ fetch('/status').then(r=>r.json()).then(d=>{ document.getElementById('pv').innerText=d.pvolt.toFixed(2); }); }, 1000);</script></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleTestCmd() {
    if (!checkAuth()) return;
    String act = webServer.arg("act");
    if (act == "on") { evse.enableCurrentTest(true); webServer.send(200, "text/plain", "Enabled"); }
    else if (act == "off") { evse.enableCurrentTest(false); webServer.send(200, "text/plain", "Disabled"); }
    else if (act == "pwm") {
        float duty = webServer.arg("val").toFloat();
        float amps = pilot.dutyToAmps(duty);
        evse.setCurrentTest(amps);
        webServer.send(200, "text/plain", String(amps));
    } else webServer.send(400, "text/plain", "Bad Request");
}

void WebController::handleWifiScan() {
    if (!apMode && !checkAuth()) return;
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    webServer.send(200, "application/json", json);
}

void WebController::handleFactoryReset() {
    if (!checkAuth()) return;
    evse.stopCharging(); pilot.disable();
    String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
    h += "<h1>Factory Reset</h1><p>Stopping Charge, Wiping WiFi/Settings, Rebooting...</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
    webServer.send(200, "text/html", h);
    // Wipe settings
    AppConfig empty; saveConfig(empty); // Or use Preferences::clear() if exposed
    WiFi.disconnect(true, true);
    requestReboot();
}

void WebController::handleWifiReset() {
    if (!checkAuth()) return;
    config.wifiSsid = ""; config.wifiPass = ""; config.useStatic = false;
    saveConfig(config);
    String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
    h += "<h1>WiFi Reset</h1><p>Credentials cleared. Rebooting into AP Mode...</p><a href='/' class='btn'>RETURN HOME</a></body></html>";
    webServer.send(200, "text/html", h);
    WiFi.disconnect(true, true); 
    requestReboot();
}

void WebController::handleEvseReset() {
    if (!checkAuth()) return;
    config.maxCurrent = 32.0f; config.rcmEnabled = true; config.allowBelow6AmpCharging = false; config.lowLimitResumeDelayMs = 300000UL;
    saveConfig(config);
    ChargingSettings cs; cs.maxCurrent = config.maxCurrent; cs.disableAtLowLimit = !config.allowBelow6AmpCharging; cs.lowLimitResumeDelayMs = config.lowLimitResumeDelayMs;
    evse.setup(cs); evse.setRcmEnabled(config.rcmEnabled);
    webServer.sendHeader("Location", "/settings", true); webServer.send(302, "text/plain", "");
}

void WebController::handleUpdate() {
    if(!checkAuth()) return;
    String h = "<html><head>"+String(dashStyle)+"</head><body><div class='container'><h1>OTA UPDATE</h1><form method='POST' action='/doUpdate' enctype='multipart/form-data' onsubmit=\"var b=document.getElementById('btn');b.disabled=true;b.value='FLASHING';var d=0;setInterval(function(){d=(d+1)%4;var t='FLASHING';for(var i=0;i<d;i++)t+='.';b.value=t;},500);\">";
    h += "<input type='file' name='update' style='margin:20px 0;' required><br><input type='submit' id='btn' value='FLASH' class='btn'></form>";
    h += "<a class='btn' style='background:#444; color:#fff;' href='/settings'>CANCEL</a></div></body></html>";
    webServer.send(200, "text/html", h);
}

void WebController::handleDoUpdate() {
    logger.info("[OTA] Upload complete, sending response");
    String h = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='15;url=/'><meta name='viewport' content='width=device-width'><style>body{background:#121212;color:#ffcc00;font-family:sans-serif;text-align:center;padding:50px;} .btn{background:#ffcc00;color:#121212;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:bold;display:inline-block;margin-top:20px;}</style></head><body>";
    if (Update.hasError()) h += "<h1>Update Failed</h1><p>Please try again.</p><a href='/update' class='btn'>TRY AGAIN</a> <a href='/' class='btn'>HOME</a>";
    else h += "<h1>Update Successful!</h1><p>Device is rebooting... You will be redirected shortly.</p><a href='/' class='btn'>RETURN HOME</a>";
    h += "</body></html>";
    webServer.send(200, "text/html", h);
    if (!Update.hasError()) { requestReboot(); }
}

void WebController::handleUpdateUpload() {
    esp_task_wdt_reset(); // Keep watchdog happy during long uploads
    HTTPUpload& u = webServer.upload();
    if(u.status == UPLOAD_FILE_START){ 
        logger.info("[OTA] Upload Start");

        g_otaUpdating = true;
        delay(100); // Give the high-priority EVSE task time to clean up and exit
        logger.info("[OTA] EVSE Task Stopped");

        logger.info("[OTA] Stopping Charge...");
        evse.stopCharging();
        logger.info("[OTA] Disabling Pilot...");
        pilot.stop();
        logger.infof("[OTA] Starting Update for file: %s", u.filename.c_str());
        if(!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            logger.error("[OTA] Update.begin failed");
        } else {
            logger.info("[OTA] Update.begin OK");
        }
    } 
    else if(u.status == UPLOAD_FILE_WRITE){ 
        if(Update.write(u.buf, u.currentSize) != u.currentSize) { Update.printError(Serial); logger.error("[OTA] Write failed"); }
    } 
    else if(u.status == UPLOAD_FILE_END){ 
        logger.infof("[OTA] Upload End: %u bytes", u.totalSize);
        esp_task_wdt_reset(); // Ensure WDT is fed before final verification
        if(Update.end(true)) {
            logger.info("[OTA] Update Successful");
        } else {
            Update.printError(Serial);
            logger.error("[OTA] Update.end failed");
        }
    }
}
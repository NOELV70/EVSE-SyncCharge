/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Storage for HTML, CSS, and JavaScript content strings used by the
 *              WebController to serve the embedded web interface.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "WebPages.h"

// --- THEME DEFINITIONS (Colors Only) ---
const char* themeYellow = R"rawliteral(
<style>:root{--bg:#121212;--fg:#eee;--c-bg:#1e1e1e;--c-bd:#ffcc00;--acc:#ffcc00;--acc-fg:#121212;--dim:#888;--stat-bg:#2a2a2a;--stat-fg:#ffcc00;--diag-bg:#1a2a2a;--diag-fg:#00ffcc;--diag-bd:#00ffcc;--in-bg:#151515;--in-bd:#333;--btn-red:#cc3300;}</style>
)rawliteral";

const char* themeBlue = R"rawliteral(
<style>:root{--bg:#f0f2f5;--fg:#333;--c-bg:#ffffff;--c-bd:#007bff;--acc:#007bff;--acc-fg:#fff;--dim:#666;--stat-bg:#e3f2fd;--stat-fg:#0d47a1;--diag-bg:#e0f7fa;--diag-fg:#006064;--diag-bd:#00bcd4;--in-bg:#fff;--in-bd:#ccc;--btn-red:#dc3545;}</style>
)rawliteral";

const char* themeDarkBlue = R"rawliteral(
<style>:root{--bg:#0d1117;--fg:#c9d1d9;--c-bg:#161b22;--c-bd:#1f6feb;--acc:#1f6feb;--acc-fg:#f0f6fc;--dim:#8b949e;--stat-bg:#0d1117;--stat-fg:#58a6ff;--diag-bg:#0d1117;--diag-fg:#3fb950;--diag-bd:#238636;--in-bg:#0d1117;--in-bd:#30363d;--btn-red:#da3633;}</style>
)rawliteral";

const char* themeGreen = R"rawliteral(
<style>:root{--bg:#f0f2f5;--fg:#333;--c-bg:#ffffff;--c-bd:#00c853;--acc:#00c853;--acc-fg:#fff;--dim:#666;--stat-bg:#e8f5e9;--stat-fg:#1b5e20;--diag-bg:#e0f2f1;--diag-fg:#00695c;--diag-bd:#4db6ac;--in-bg:#fff;--in-bd:#ccc;--btn-red:#d32f2f;}</style>
)rawliteral";

const char* themeDarkGreen = R"rawliteral(
<style>:root{--bg:#0d1117;--fg:#e6fffa;--c-bg:#161b22;--c-bd:#00e676;--acc:#00e676;--acc-fg:#000;--dim:#8b949e;--stat-bg:#0d1117;--stat-fg:#00e676;--diag-bg:#0d1117;--diag-fg:#69f0ae;--diag-bd:#00c853;--in-bg:#0d1117;--in-bd:#30363d;--btn-red:#ff5252;}</style>
)rawliteral";

// --- COMMON LAYOUT (Uses Variables) ---
const char* dashStyle = R"rawliteral(
<style>
* { box-sizing: border-box; }
body { background: var(--bg); color: var(--fg); font-family: 'Segoe UI', sans-serif; text-align: center; padding: 20px; }
.container { background: var(--c-bg); border: 2px solid var(--c-bd); display: inline-block; padding: 30px; border-radius: 12px; width: 100%; max-width: 560px; box-shadow: 0 10px 40px rgba(0,0,0,0.5); }
.head-wrap { display: flex; align-items: center; justify-content: center; gap: 20px; border-bottom: 2px solid var(--acc); padding-bottom: 15px; margin-bottom: 15px; }
.head-wrap h1 { border-bottom: none; padding-bottom: 0; text-align: left; }
.head-wrap .version-tag { margin-bottom: 0; text-align: left; margin-top: 5px; }
.logo { width: 70px; height: 70px; fill: var(--acc); stroke: var(--acc); }
h1 { color: var(--acc); margin: 0; letter-spacing: 2px; text-transform: uppercase; font-size: 1.6em; border-bottom: 2px solid var(--acc); padding-bottom: 5px; }
.version-tag { color: var(--acc); font-family: monospace; font-size: 0.85em; margin-bottom: 10px; display: block; letter-spacing: 1px; }
.stat { background: var(--stat-bg); padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid var(--acc); text-align: left; color: var(--stat-fg); font-family: monospace; font-size: 0.9em; }
.diag-header { color: var(--dim); font-size: 0.7em; text-transform: uppercase; text-align: left; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }
.stat-diag { background: var(--diag-bg); padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid var(--diag-bd); text-align: left; color: var(--diag-fg); font-family: monospace; font-size: 0.9em; }
.btn { color: var(--acc-fg); background: var(--acc); padding: 12px; border-radius: 6px; font-weight: bold; text-decoration: none; display: inline-block; margin-top: 10px; border: none; cursor: pointer; text-align: center; font-size: 0.95em; width:100%; transition: 0.3s; }
.btn:hover { opacity: 0.8; }
.btn-red { background: var(--btn-red); color: #fff; }
.footer { color: var(--dim); font-size: 0.95em; margin-top: 25px; border-top: 1px solid var(--in-bd); padding-top: 15px; font-family: monospace; text-align: center; }
label { display:block; text-align:left; margin-top:10px; color:var(--dim); }
input,select { width:100%; padding:10px; border-radius:6px; border:1px solid var(--in-bd); background:var(--in-bg); color:var(--fg); margin-top:6px; transition: 0.3s; }
input:disabled { opacity: 0.5; cursor: not-allowed; }
.modal { display: none; position: fixed; z-index: 1000; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.8); backdrop-filter: blur(3px); }
.modal-content { background-color: var(--c-bg); margin: 15% auto; padding: 25px; border: 2px solid var(--c-bd); width: 90%; max-width: 400px; border-radius: 12px; text-align: center; box-shadow: 0 0 30px rgba(0,0,0,0.5); }
</style>
)rawliteral";

const char* dynamicScript = R"rawliteral(
<script>
function toggleStaticFields() {
  var isStatic = document.getElementById('mode').value == '1';
  var fields = ['ip', 'gw', 'sn'];
  fields.forEach(function(f) { document.getElementById(f).disabled = !isStatic; });
}
window.onload = toggleStaticFields;
</script>
)rawliteral";

const char* ajaxScript = R"rawliteral(
<script>
setInterval(function(){
fetch('/status?t='+Date.now()).then(r=>r.json()).then(d=>{
document.getElementById('vst').innerText=d.vst;
document.getElementById('clim').innerText=d.clim.toFixed(1);
document.getElementById('pwm').innerText=d.pwm;
document.getElementById('pvolt').innerText=d.pvolt.toFixed(2);
document.getElementById('acrel').innerText=d.acrel;
document.getElementById('phase').innerText=d.phase;
document.getElementById('upt').innerText=d.upt;
document.getElementById('rssi').innerText=d.rssi;
var l=document.getElementById('lock');if(l){l.innerText=d.lock?'YES':'NO';l.style.color=d.lock?'#ff5252':'#00ffcc';}

var bStart=document.getElementById('btn-start');
var bPause=document.getElementById('btn-pause');
function setEn(b,en){
 if(en){b.disabled=false;b.style.opacity='1';b.style.cursor='pointer';b.style.background='';b.style.color='';}
 else{b.disabled=true;b.style.opacity='1';b.style.cursor='not-allowed';b.style.background='#333';b.style.color='#777';}
}
if(bStart){
 var canStart = d.conn && d.state!=1 && !d.paused;
 setEn(bStart, canStart);
 bStart.style.display='inline-block';
}
if(bPause){
 bPause.style.display='inline-block';
 if(d.state==1){
  setEn(bPause,d.conn);
  if(d.conn){bPause.style.background='#ff9800';bPause.style.color='#fff';}
  bPause.innerText='PAUSE CHARGING';
  bPause.onclick=function(){confirmCmd('pause',this)};
 }else if(d.paused){
  setEn(bPause,d.conn);
  if(d.conn){bPause.style.background='#4caf50';bPause.style.color='#fff';}
  bPause.innerText='RESUME CHARGING';
  bPause.onclick=function(){confirmCmd('start',this)};
 }else{
  bPause.innerText='PAUSE CHARGING';
  setEn(bPause, false);
 }
}
});},1000);
</script>
)rawliteral";

const char* logoSvg = R"rawliteral(
<svg class='logo' viewBox='0 0 100 100'>
<path d='M10 50 L50 10 L90 50 V90 H10 Z' fill='none' stroke-width='4'/>
<path d='M30 75 Q30 65 50 65 Q70 65 70 75 L73 82 H27 Z'/>
<path d='M45 25 L35 50 H50 L40 75 L65 40 H50 L60 25 Z' stroke='var(--bg)' stroke-width='1'/>
</svg>
)rawliteral";
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

const char* dashStyle = R"rawliteral(
<style>
* { box-sizing: border-box; }
body { background: #121212; color: #eee; font-family: 'Segoe UI', sans-serif; text-align: center; padding: 20px; }
.container { background: #1e1e1e; border: 2px solid #ffcc00; display: inline-block; padding: 30px; border-radius: 12px; width: 100%; max-width: 560px; box-shadow: 0 10px 40px rgba(0,0,0,0.8); }
.logo { width: 80px; height: 80px; margin: 0 auto 10px; display: block; fill: #ffcc00; }
h1 { color: #ffcc00; margin: 0; letter-spacing: 2px; text-transform: uppercase; font-size: 1.6em; border-bottom: 2px solid #ffcc00; padding-bottom: 5px; }
.version-tag { color: #ffcc00; font-family: monospace; font-size: 0.85em; margin-bottom: 10px; display: block; letter-spacing: 1px; }
.stat { background: #2a2a2a; padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid #ffcc00; text-align: left; color: #ffcc00; font-family: monospace; font-size: 0.9em; }
.diag-header { color: #888; font-size: 0.7em; text-transform: uppercase; text-align: left; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }
.stat-diag { background: #1a2a2a; padding: 12px; margin: 10px 0; border-radius: 6px; border-left: 6px solid #00ffcc; text-align: left; color: #00ffcc; font-family: monospace; font-size: 0.9em; }
.btn { color: #121212; background: #ffcc00; padding: 12px; border-radius: 6px; font-weight: bold; text-decoration: none; display: inline-block; margin-top: 10px; border: none; cursor: pointer; text-align: center; font-size: 0.95em; width:100%; transition: 0.3s; }
.btn:hover { opacity: 0.8; }
.btn-red { background: #cc3300; color: #fff; }
.footer { color: #666; font-size: 0.95em; margin-top: 25px; border-top: 1px solid #333; padding-top: 15px; font-family: monospace; text-align: center; }
label { display:block; text-align:left; margin-top:10px; color:#ccc; }
input,select { width:100%; padding:10px; border-radius:6px; border:1px solid #333; background:#151515; color:#eee; margin-top:6px; transition: 0.3s; }
input:disabled { background: #0f0f0f; color: #444; border-color: #222; opacity: 0.5; cursor: not-allowed; }
.modal { display: none; position: fixed; z-index: 1000; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.88); backdrop-filter: blur(3px); }
.modal-content { background-color: #1e1e1e; margin: 15% auto; padding: 25px; border: 2px solid #ffcc00; width: 90%; max-width: 400px; border-radius: 12px; text-align: center; box-shadow: 0 0 30px rgba(0,0,0,0.8); }
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
<path d='M10 50 L50 10 L90 50 V90 H10 Z' fill='none' stroke='#ffcc00' stroke-width='4'/>
<path d='M30 75 Q30 65 50 65 Q70 65 70 75 L73 82 H27 Z' fill='#ffcc00'/>
<path d='M45 25 L35 50 H50 L40 75 L65 40 H50 L60 25 Z' fill='#ffcc00' stroke='#121212' stroke-width='1'/>
</svg>
)rawliteral";
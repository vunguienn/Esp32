#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

const char DASHBOARD_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Grow Gateway</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#0f172a;color:#e2e8f0;min-height:100vh}
.hdr{background:#1e293b;padding:10px 20px;border-bottom:1px solid #334155;display:flex;justify-content:space-between;align-items:center;position:sticky;top:0;z-index:100}
.hdr h1{font-size:16px;color:#4ade80}
.hdr-i{display:flex;gap:12px;font-size:11px;color:#94a3b8}
.dt{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:3px}
.dt-on{background:#4ade80;box-shadow:0 0 6px #4ade80}
.dt-off{background:#ef4444}
.tabs{display:flex;background:#1e293b;border-bottom:1px solid #334155;overflow-x:auto}
.tb{padding:10px 18px;cursor:pointer;border-bottom:2px solid transparent;font-size:13px;color:#94a3b8;white-space:nowrap;transition:.2s}
.tb:hover{color:#e2e8f0;background:rgba(255,255,255,.03)}
.tb.act{color:#4ade80;border-bottom-color:#4ade80;background:rgba(74,222,128,.05)}
.pg{display:none;padding:15px;max-width:1400px;margin:0 auto}
.pg.act{display:block}
.gr{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:15px}
.cd{background:#1e293b;border-radius:10px;padding:16px;border:1px solid #334155}
.cd h2{font-size:11px;text-transform:uppercase;color:#64748b;margin-bottom:12px;letter-spacing:1px}
.fw{grid-column:1/-1}
.sg{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.si{background:#0f172a;padding:10px;border-radius:6px;text-align:center}
.sv{font-size:22px;font-weight:700;color:#4ade80}
.sl{font-size:10px;color:#64748b;margin-top:2px}
.st{font-size:9px;color:#475569}
.rg{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.rb{padding:12px 6px;border-radius:8px;text-align:center;cursor:pointer;transition:.2s;border:2px solid transparent;user-select:none}
.rb:hover{transform:scale(1.03);filter:brightness(1.2)}
.rb:active{transform:scale(.97)}
.r1{background:#166534;border-color:#22c55e;color:#fff}
.r0{background:#1e293b;border-color:#334155;color:#64748b}
.rn{font-size:10px;margin-top:3px}
.rc{font-weight:700;font-size:12px}
.rs{font-size:9px;margin-top:2px;font-weight:600}
.ig{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.ii{padding:10px 6px;border-radius:8px;text-align:center;border:2px solid #334155;transition:.2s}
.i1{background:#1e3a5f;border-color:#3b82f6;color:#fff}
.i0{background:#1e293b;border-color:#334155;color:#64748b}
.lb{max-height:500px;overflow-y:auto;font-family:'Cascadia Code',monospace;font-size:11px;background:#0f172a;border-radius:6px;padding:6px}
.lr{padding:3px 6px;border-left:3px solid #334155;margin-bottom:2px;display:flex;gap:6px;border-radius:2px}
.lr:hover{background:rgba(255,255,255,.02)}
.lr.INFO{border-left-color:#3b82f6}
.lr.WARN{border-left-color:#f59e0b}
.lr.ERROR{border-left-color:#ef4444}
.lt{color:#475569;min-width:65px}
.ll{font-weight:700;min-width:42px}
.ll.INFO{color:#3b82f6}
.ll.WARN{color:#f59e0b}
.ll.ERROR{color:#ef4444}
.lc{color:#10b981;min-width:50px;font-weight:600}
.lm{color:#cbd5e1;flex:1;word-break:break-word}
.fb{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:10px;background:#0f172a;border-radius:8px;margin-bottom:10px}
.fs{background:#1e293b;padding:8px 12px;border-radius:6px;border:1px solid #334155;font-size:11px;text-align:center;min-width:80px}
.fs.ok{border-color:#22c55e;color:#4ade80}
.fs.er{border-color:#ef4444;color:#ef4444}
.fs.wt{border-color:#f59e0b;color:#f59e0b}
.fa{color:#475569;font-size:14px}
.fs small{display:block;color:#64748b;font-size:9px;margin-top:2px}
.ir{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid rgba(255,255,255,.04)}
.ik{color:#64748b;font-size:12px}
.iv{color:#e2e8f0;font-size:12px;font-family:monospace}
.wb{background:#0f172a;border-radius:8px;padding:12px;margin-bottom:8px;border-left:4px solid #4ade80}
.wb.wa{border-left-color:#f59e0b;background:rgba(245,158,11,.06)}
.wt{font-weight:700;font-size:13px;color:#4ade80;margin-bottom:6px}
.wb.wa .wt{color:#f59e0b}
.wg{display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:11px}
.wi{background:rgba(255,255,255,.03);padding:5px 7px;border-radius:4px}
.wi span:first-child{color:#64748b}
.wi span:last-child{color:#e2e8f0;font-weight:600}
.bd{display:inline-block;padding:2px 7px;border-radius:10px;font-size:9px;font-weight:700}
.bd-ok{background:#166534;color:#4ade80}
.bd-er{background:#7f1d1d;color:#fca5a5}
.btn{padding:6px 14px;border:none;border-radius:5px;cursor:pointer;font-size:12px;font-weight:600}
.btn-g{background:#4ade80;color:#000}
.btn-r{background:#ef4444;color:#fff}
</style>
</head>
<body>
<div class="hdr">
 <h1>🌱 ESP32 Grow Gateway</h1>
 <div class="hdr-i">
  <span><span class="dt dt-on" id="hw"></span>WiFi <span id="hip">-</span></span>
  <span><span class="dt dt-off" id="hm"></span>MQTT</span>
  <span>⏱<span id="hup">-</span></span>
  <span>🕐<span id="htim">-</span></span>
 </div>
</div>
<div class="tabs">
 <div class="tb act" onclick="sp('overview')">📊 Overview</div>
 <div class="tb" onclick="sp('relays')">⚡ Relays & I/O</div>
 <div class="tb" onclick="sp('auto')">🤖 Automation</div>
 <div class="tb" onclick="sp('logs')">📜 Live Logs</div>
 <div class="tb" onclick="sp('sys')">⚙️ System</div>
</div>

<!-- OVERVIEW -->
<div class="pg act" id="pg-overview">
 <div class="cd fw" style="margin-bottom:15px">
  <h2>🔄 Data Flow</h2>
  <div class="fb" id="flow">
   <div class="fs" id="f0"><b>☁️Cloud</b><small>Broker</small></div><div class="fa">→</div>
   <div class="fs" id="f1"><b>📡MQTT</b><small id="f1s">-</small></div><div class="fa">→</div>
   <div class="fs" id="f2"><b>📋Parse</b><small id="f2s">-</small></div><div class="fa">→</div>
   <div class="fs" id="f3"><b>💾Save</b><small id="f3s">-</small></div><div class="fa">→</div>
   <div class="fs" id="f4"><b>🤖Exec</b><small id="f4s">-</small></div>
  </div>
 </div>
 <div class="gr">
  <div class="cd"><h2>🌡️ Sensors</h2><div class="sg" id="sen">Loading...</div></div>
  <div class="cd"><h2>⚡ Relays <span style="font-size:9px;color:#475569">(click to toggle)</span></h2><div class="rg" id="rq">Loading...</div></div>
  <div class="cd"><h2>🤖 Automation</h2><div id="aq">Loading...</div></div>
  <div class="cd"><h2>📥 Digital Inputs (7ch)</h2><div class="ig" id="iq">Loading...</div></div>
 </div>
</div>

<!-- RELAYS & I/O -->
<div class="pg" id="pg-relays">
 <div class="gr">
  <div class="cd fw">
   <h2>⚡ Relay Control — Click to Toggle ON/OFF</h2>
   <p style="color:#64748b;font-size:11px;margin-bottom:12px">Manual toggle. Auto mode changes are reflected in real-time.</p>
   <div class="rg" id="rc" style="grid-template-columns:repeat(4,1fr)">Loading...</div>
  </div>
  <div class="cd fw">
   <h2>📥 Digital Inputs Monitor — 7 Channels (Optocoupler)</h2>
   <p style="color:#64748b;font-size:11px;margin-bottom:12px">Real-time state. Active = signal detected at input pin.</p>
   <div class="ig" id="im" style="grid-template-columns:repeat(7,1fr)">Loading...</div>
  </div>
  <div class="cd fw">
   <h2>📝 I/O Activity</h2>
   <div class="lb" id="iol" style="max-height:250px">No activity yet</div>
  </div>
 </div>
</div>

<!-- AUTOMATION -->
<div class="pg" id="pg-auto">
 <div class="gr">
  <div class="cd fw"><h2>📊 Automation Status</h2><div id="ad">Loading...</div></div>
  <div class="cd fw"><h2>📅 Weekly Plans</h2><div id="wp" style="max-height:600px;overflow-y:auto">Loading...</div></div>
 </div>
</div>

<!-- LOGS -->
<div class="pg" id="pg-logs">
 <div class="cd fw">
  <h2>📜 Live System Logs <span style="float:right;font-size:10px;color:#475569">Auto-scroll | <span id="lcnt">0</span> entries</span></h2>
  <div class="lb" id="ll" style="max-height:calc(100vh - 180px)">Loading...</div>
 </div>
</div>

<!-- SYSTEM -->
<div class="pg" id="pg-sys">
 <div class="gr">
  <div class="cd"><h2>🔧 Hardware</h2><div id="shw">Loading...</div></div>
  <div class="cd"><h2>📡 Network</h2><div id="snt">Loading...</div></div>
  <div class="cd"><h2>💾 Storage</h2><div id="sst">Loading...</div></div>
  <div class="cd"><h2>🛡️ Watchdog</h2><div id="swd">Loading...</div></div>
 </div>
</div>

<script>
let ap='overview',iol=[];
function sp(p){ap=p;document.querySelectorAll('.pg').forEach(e=>e.classList.remove('act'));document.getElementById('pg-'+p).classList.add('act');document.querySelectorAll('.tb').forEach((e,i)=>{e.classList.remove('act');if(i===(['overview','relays','auto','logs','sys'].indexOf(p)))e.classList.add('act')});ua()}
async function $g(e){try{const r=await fetch('/api/'+e);return await r.json()}catch(x){return null}}
async function $p(e){try{const r=await fetch('/api/'+e,{method:'POST'});return await r.json()}catch(x){return null}}
function fm(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return d+'d'+h+'h'+m+'m'}
function cl(v,l,h){return v<l?'#f59e0b':v>h?'#ef4444':'#4ade80'}

async function tr(c){const r=await $p('relay/toggle?ch='+c);if(r&&r.success){iol.unshift({t:Date.now(),m:'Relay CH'+c+' → '+(r.state?'ON':'OFF')+' (manual)'});if(iol.length>50)iol.pop();uR();uIO()}}

async function uH(){
 const d=await $g('status');if(!d)return;
 document.getElementById('hip').textContent=d.wifiIP;
 document.getElementById('hup').textContent=fm(d.uptime);
 document.getElementById('htim').textContent=d.localTime||'-';
 document.getElementById('hw').className='dt '+(d.wifiRSSI<0?'dt-on':'dt-off');
 document.getElementById('hm').className='dt '+(d.mqttConnected?'dt-on':'dt-off');
 return d;
}

async function uS(){
 const d=await $g('sensors');if(!d)return;const t=d.targets||{};
 document.getElementById('sen').innerHTML=
  `<div class="si"><div class="sv" style="color:${cl(d.temperature,t.tempTarget-2,t.tempTarget+2)}">${d.temperature.toFixed(1)}°C</div><div class="sl">Temp</div><div class="st">Tgt: ${t.tempTarget?.toFixed(1)||'-'}°C</div></div>`+
  `<div class="si"><div class="sv" style="color:${cl(d.humidity,t.humiLow,t.humiHigh)}">${d.humidity.toFixed(1)}%</div><div class="sl">Humidity</div><div class="st">${t.humiLow||'-'}-${t.humiHigh||'-'}%</div></div>`+
  `<div class="si"><div class="sv" style="color:${cl(d.co2,t.co2Start,t.co2Stop)}">${d.co2}</div><div class="sl">CO₂ ppm</div><div class="st">${t.co2Start||'-'}-${t.co2Stop||'-'}</div></div>`+
  `<div class="si"><div class="sv">${d.vpd.toFixed(2)}</div><div class="sl">VPD kPa</div><div class="st">0.8-1.2</div></div>`+
  `<div class="si"><div class="sv">${d.waterTemp.toFixed(1)}°C</div><div class="sl">Water</div></div>`+
  `<div class="si"><div class="sv">${d.ec.toFixed(2)}</div><div class="sl">EC</div></div>`+
  `<div class="si"><div class="sv">${d.ph.toFixed(1)}</div><div class="sl">pH</div></div>`+
  `<div class="si"><div class="sv">${d.waterLevel}%</div><div class="sl">Level</div></div>`;
}

async function uR(){
 const d=await $g('relays');if(!d||!d.relays)return;
 const h=d.relays.map(r=>`<div class="rb ${r.state?'r1':'r0'}" onclick="tr(${r.channel})"><div class="rc">CH${r.channel}</div><div class="rn">${r.name}</div><div class="rs">${r.state?'🟢 ON':'⚫ OFF'}</div></div>`).join('');
 document.getElementById('rq').innerHTML=h;
 const rc=document.getElementById('rc');if(rc)rc.innerHTML=h;
}

async function uI(){
 const d=await $g('inputs');if(!d||!d.inputs)return;
 const h=d.inputs.map(i=>`<div class="ii ${i.state?'i1':'i0'}"><div style="font-weight:700;font-size:12px">IN${i.channel}</div><div style="font-size:10px;margin-top:3px">${i.state?'🔵 ACTIVE':'⚫ IDLE'}</div><div style="font-size:9px;color:#475569;margin-top:2px">GPIO${i.pin}</div></div>`).join('');
 document.getElementById('iq').innerHTML=h;
 const im=document.getElementById('im');if(im)im.innerHTML=h;
}

async function uAQ(){
 const d=await $g('automation');if(!d)return;
 document.getElementById('aq').innerHTML=
  `<div class="ir"><span class="ik">Status</span><span class="iv"><span class="bd ${d.loaded?'bd-ok':'bd-er'}">${d.loaded?'LOADED':'NOT LOADED'}</span></span></div>`+
  `<div class="ir"><span class="ik">Version</span><span class="iv">v${d.version}</span></div>`+
  `<div class="ir"><span class="ik">Week</span><span class="iv" style="color:#f59e0b">W${d.currentWeek} - ${d.currentPhase}</span></div>`+
  `<div class="ir"><span class="ik">Rules</span><span class="iv">${d.ruleCount}</span></div>`+
  `<div class="ir"><span class="ik">Light</span><span class="iv">☀️${d.lighting?.lightsOn||'-'} → 🌙${d.lighting?.lightsOff||'-'}</span></div>`+
  `<div class="ir"><span class="ik">Mode</span><span class="iv">${d.isDaytime?'☀️ DAY':'🌙 NIGHT'}</span></div>`;
}

async function uF(){
 const d=await $g('automation/check');const s=await $g('status');if(!s)return;
 const mc=s.mqttConnected;
 document.getElementById('f0').className='fs '+(mc?'ok':'wt');
 document.getElementById('f1').className='fs '+(mc?'ok':'er');
 document.getElementById('f1s').textContent=mc?'Connected':'Disconnected';
 if(d){
  document.getElementById('f2').className='fs '+(d.automation_loaded?'ok':'wt');
  document.getElementById('f2s').textContent=d.automation_loaded?'v'+d.automation_version:'Waiting';
  document.getElementById('f3').className='fs '+(d.file_exists?'ok':'wt');
  document.getElementById('f3s').textContent=d.file_exists?(d.file_size/1024).toFixed(1)+'KB':'No file';
  document.getElementById('f4').className='fs '+(d.automation_loaded?'ok':'wt');
  document.getElementById('f4s').textContent=d.automation_loaded?d.automation_rules+' rules':'Idle';
 }
}

async function uAD(){
 const d=await $g('automation/full');if(!d)return;
 document.getElementById('ad').innerHTML=
  `<div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px">`+
  `<div class="si"><div class="sv" style="font-size:14px;color:${d.loaded?'#4ade80':'#ef4444'}">${d.loaded?'✅ LOADED':'❌ NOT LOADED'}</div><div class="sl">Status</div></div>`+
  `<div class="si"><div class="sv" style="font-size:14px">v${d.version}</div><div class="sl">Version</div></div>`+
  `<div class="si"><div class="sv" style="font-size:14px;color:#f59e0b">W${d.currentWeek}</div><div class="sl">${d.currentPhase}</div></div>`+
  `<div class="si"><div class="sv" style="font-size:14px">${d.ruleCount}</div><div class="sl">Rules</div></div>`+
  `<div class="si"><div class="sv" style="font-size:14px">${d.totalWeeks}</div><div class="sl">Total Weeks</div></div>`+
  `<div class="si"><div class="sv" style="font-size:12px">${d.timezoneName||'-'}</div><div class="sl">TZ</div></div></div>`;
 let w='';
 if(d.weeklyPlans&&d.weeklyPlans.length>0){
  d.weeklyPlans.forEach(p=>{
   const a=p.week===d.currentWeek;
   w+=`<div class="wb ${a?'wa':''}"><div class="wt">${a?'⭐ ':''}Week ${p.week}: ${p.phase}${a?' (ACTIVE)':''}</div><div class="wg">`+
   `<div class="wi"><span>💡ON:</span> <span>${p.lighting.lightsOn}</span></div>`+
   `<div class="wi"><span>💡OFF:</span> <span>${p.lighting.lightsOff}</span></div>`+
   `<div class="wi"><span>☀️Temp:</span> <span>${p.targets.tempTargetDay}°C</span></div>`+
   `<div class="wi"><span>🌙Temp:</span> <span>${p.targets.tempTargetNight}°C</span></div>`+
   `<div class="wi"><span>💧HumiD:</span> <span>${p.targets.humiLowDay}-${p.targets.humiHighDay}%</span></div>`+
   `<div class="wi"><span>💧HumiN:</span> <span>${p.targets.humiLowNight}-${p.targets.humiHighNight}%</span></div>`+
   `<div class="wi"><span>🌿CO₂D:</span> <span>${p.targets.co2StartDay}-${p.targets.co2StopDay}</span></div>`+
   `<div class="wi"><span>🌿CO₂N:</span> <span>${p.targets.co2StartNight}-${p.targets.co2StopNight}</span></div>`+
   `<div class="wi"><span>🌀Circ:</span> <span>${p.equipment.fanCircMode}</span></div>`+
   `<div class="wi"><span>🌀Exh:</span> <span>${p.equipment.fanExhMode}</span></div>`+
   `<div class="wi"><span>❄️AC:</span> <span>${p.equipment.acMode} ${p.equipment.acTargetTemp}°C</span></div>`+
   `<div class="wi"><span>📊VPD:</span> <span>${p.targets.vpdMin}-${p.targets.vpdMax}</span></div>`+
   `</div></div>`;
  });
 }else{w='<div style="text-align:center;color:#475569;padding:30px">No weekly plans — Send automation from Cloud first</div>';}
 document.getElementById('wp').innerHTML=w;
}

async function uL(){
 const d=await $g('logs');if(!d||!d.logs)return;
 document.getElementById('lcnt').textContent=d.count;
 const h=d.logs.slice().reverse().map(l=>{
  const t=fm(Math.floor(l.timestamp/1000));
  return `<div class="lr ${l.level}"><span class="lt">${t}</span><span class="ll ${l.level}">${l.level}</span><span class="lc">${l.category}</span><span class="lm">${l.message}</span></div>`;
 }).join('')||'<div style="color:#475569;text-align:center;padding:20px">No logs yet</div>';
 const b=document.getElementById('ll');
 const ab=b.scrollHeight-b.scrollTop<=b.clientHeight+60;
 b.innerHTML=h;if(ab)b.scrollTop=b.scrollHeight;
}

function uIO(){
 const e=document.getElementById('iol');if(!e)return;
 e.innerHTML=iol.map(x=>{
  const t=new Date(x.t).toLocaleTimeString();
  return `<div class="lr INFO"><span class="lt">${t}</span><span class="lm">${x.m}</span></div>`;
 }).join('')||'<div style="color:#475569;text-align:center;padding:15px">No I/O activity</div>';
}

async function uSY(){
 const d=await $g('system');if(!d)return;
 const hp=((d.heapSize-d.freeHeap)/d.heapSize*100).toFixed(1);
 const sv=(d.storage.usedBytes/d.storage.totalBytes*100).toFixed(1);
 document.getElementById('shw').innerHTML=
  `<div class="ir"><span class="ik">Chip</span><span class="iv">${d.chipModel}</span></div>`+
  `<div class="ir"><span class="ik">Cores</span><span class="iv">${d.chipCores}@${d.cpuFreqMHz}MHz</span></div>`+
  `<div class="ir"><span class="ik">Heap</span><span class="iv" style="color:${d.freeHeap<50000?'#ef4444':'#4ade80'}">${(d.freeHeap/1024).toFixed(1)}KB (${hp}%)</span></div>`+
  `<div class="ir"><span class="ik">MinHeap</span><span class="iv">${(d.minFreeHeap/1024).toFixed(1)}KB</span></div>`+
  `<div class="ir"><span class="ik">Flash</span><span class="iv">${(d.flashSize/1048576).toFixed(1)}MB</span></div>`+
  `<div class="ir"><span class="ik">Sketch</span><span class="iv">${(d.sketchSize/1024).toFixed(0)}KB</span></div>`+
  `<div class="ir"><span class="ik">Uptime</span><span class="iv">${d.uptimeFormatted}</span></div>`;
 document.getElementById('snt').innerHTML=
  `<div class="ir"><span class="ik">SSID</span><span class="iv">${d.wifi.ssid}</span></div>`+
  `<div class="ir"><span class="ik">IP</span><span class="iv">${d.wifi.ip}</span></div>`+
  `<div class="ir"><span class="ik">MAC</span><span class="iv">${d.wifi.mac}</span></div>`+
  `<div class="ir"><span class="ik">RSSI</span><span class="iv">${d.wifi.rssi}dBm</span></div>`+
  `<div class="ir"><span class="ik">CH</span><span class="iv">${d.wifi.channel}</span></div>`;
 document.getElementById('sst').innerHTML=
  `<div class="ir"><span class="ik">Total</span><span class="iv">${(d.storage.totalBytes/1024).toFixed(0)}KB</span></div>`+
  `<div class="ir"><span class="ik">Used</span><span class="iv">${(d.storage.usedBytes/1024).toFixed(1)}KB (${sv}%)</span></div>`+
  `<div class="ir"><span class="ik">Free</span><span class="iv">${(d.storage.freeBytes/1024).toFixed(1)}KB</span></div>`;
 const w=await $g('watchdog');if(w){
  document.getElementById('swd').innerHTML=
   `<div class="ir"><span class="ik">Health</span><span class="iv"><span class="bd" style="background:${w.healthColor}40;color:${w.healthColor}">${w.health}</span></span></div>`+
   `<div class="ir"><span class="ik">Crashes</span><span class="iv">${w.crashCount}/${w.maxCrashCount}</span></div>`+
   `<div class="ir"><span class="ik">SafeMode</span><span class="iv">${w.safeMode?'⚠️YES':'✅NO'}</span></div>`+
   `<div class="ir"><span class="ik">LastReset</span><span class="iv">${w.lastReset}</span></div>`;
 }
}

async function ua(){
 await uH();
 if(ap==='overview')await Promise.all([uS(),uR(),uI(),uAQ(),uF()]);
 else if(ap==='relays'){await Promise.all([uR(),uI()]);uIO()}
 else if(ap==='auto')await uAD();
 else if(ap==='logs')await uL();
 else if(ap==='sys')await uSY();
}
ua();setInterval(ua,2000);
</script>
</body>
</html>
)rawliteral";

#endif

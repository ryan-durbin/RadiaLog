#pragma once
#include <pgmspace.h>

// Settings page — edit WiFi, token, device config
const char SETTINGS_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RadiaLog — Settings</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0d1117;color:#c9d1d9;font-size:16px;line-height:1.5}
a{color:#58a6ff;text-decoration:none}a:hover{text-decoration:underline}
nav{background:#161b22;border-bottom:1px solid #30363d;padding:0 1rem;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.1rem;color:#f0f6fc;flex:1}
.nav-links{display:flex;gap:0.25rem;margin:0 0.75rem}
.nav-links a{color:#8b949e;padding:0.35rem 0.6rem;border-radius:6px;font-size:0.875rem;white-space:nowrap}
.nav-links a:hover,.nav-links a.active{color:#f0f6fc;background:#21262d;text-decoration:none}
main{max-width:600px;margin:0 auto;padding:1rem}
.section-title{font-size:0.875rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin:1.5rem 0 0.5rem;border-bottom:1px solid #21262d;padding-bottom:0.25rem}
.section-title:first-child{margin-top:0.5rem}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:1rem;margin-bottom:0.75rem}
label{display:block;font-size:0.8rem;color:#8b949e;margin-bottom:0.25rem}
input[type="text"],input[type="password"],input[type="number"]{width:100%;padding:0.4rem 0.6rem;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:0.875rem;margin-bottom:0.5rem}
input:focus{outline:none;border-color:#58a6ff}
.row{display:flex;gap:0.5rem;align-items:flex-end}
.row>*{flex:1}
.btn{display:inline-block;padding:0.4rem 0.8rem;border-radius:6px;border:1px solid #30363d;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:0.8rem}
.btn:hover{background:#30363d}
.btn-primary{background:#238636;border-color:#238636;color:#fff}
.btn-primary:hover{background:#2ea043}
.btn-danger{background:#da3633;border-color:#da3633;color:#fff}
.btn-danger:hover{background:#f85149}
.btn-sm{padding:0.25rem 0.5rem;font-size:0.75rem}
.wifi-entry{display:flex;align-items:center;gap:0.5rem;margin-bottom:0.5rem}
.wifi-entry span{flex:1;font-size:0.875rem}
.toast{display:none;position:fixed;bottom:1rem;left:50%;transform:translateX(-50%);background:#238636;color:#fff;padding:0.5rem 1rem;border-radius:6px;font-size:0.875rem;z-index:200}
.toast.error{background:#da3633}
.actions-grid{display:flex;flex-wrap:wrap;gap:0.5rem;margin-top:0.5rem}
.nav-status{display:flex;align-items:center;gap:0.5rem;flex-shrink:0}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;flex-shrink:0}
.dot-green{background:#3fb950}.dot-red{background:#f85149}.dot-gray{background:#484f58}
.status-popup{display:none;position:fixed;top:56px;right:1rem;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:0.75rem 1rem;z-index:200;min-width:200px;box-shadow:0 4px 12px rgba(0,0,0,0.4);font-size:0.8rem}
.status-popup .sr{display:flex;justify-content:space-between;align-items:center;padding:0.25rem 0}
.status-popup .sl{color:#8b949e}.st-ok{color:#3fb950}.st-err{color:#f85149}.st-warn{color:#d29922}
</style>
</head>
<body>
<nav>
  <span class="nav-brand">&#9762; RadiaLog</span>
  <div class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/debug">Debug</a>
    <a href="/settings" class="active">Settings</a>
    <a href="/data">Data</a>
  </div>
  <div class="nav-status" style="cursor:pointer">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="USB"></span>
  </div>
</nav>
<div class="status-popup" id="status-popup"></div>
<main>

<p class="section-title">WiFi Networks</p>
<div id="wifi-list"></div>
<div class="card" id="wifi-add">
  <div class="row">
    <div><label>SSID</label><input type="text" id="new-ssid"></div>
    <div><label>Password</label><input type="password" id="new-pass"></div>
    <div style="flex:0"><label>&nbsp;</label><button class="btn" onclick="addWifi()">Add</button></div>
  </div>
</div>

<p class="section-title">RadiaMaps</p>
<div class="card">
  <label>Device Token</label>
  <input type="password" id="cfg-token">
  <label>Upload URL</label>
  <input type="text" id="cfg-url">
  <label>Device ID</label>
  <input type="text" id="cfg-device-id">
</div>

<p class="section-title">BLE RadiaCode Devices</p>
<div id="ble-list"></div>
<div class="card" id="ble-scan-card">
  <div style="display:flex;align-items:center;gap:0.75rem;margin-bottom:0.5rem">
    <button class="btn" onclick="scanBle()" id="ble-scan-btn">Scan for RadiaCode</button>
    <span id="ble-scan-status" style="font-size:0.8rem;color:#8b949e"></span>
  </div>
  <div id="ble-scan-results"></div>
  <span style="font-size:0.75rem;color:#8b949e">Scans for nearby RadiaCode devices via Bluetooth. Tap a device to add it. Up to 4 devices.</span>
</div>

<p class="section-title">Geolocation</p>
<div class="card">
  <label>Google Geolocation API Key</label>
  <input type="password" id="cfg-google-key">
  <span style="font-size:0.75rem;color:#8b949e">Used for WiFi-based location when GPS has no fix. Leave empty to disable.</span>
</div>

<p class="section-title">Device</p>
<div class="card">
  <label>Device Name</label>
  <input type="text" id="cfg-name">
  <label>Reading Interval (ms)</label>
  <input type="number" id="cfg-interval" min="500" max="60000" step="100">
  <label>AP Password (empty = open)</label>
  <input type="text" id="cfg-ap-pass">
</div>

<div style="margin-top:1rem;text-align:right">
  <button class="btn btn-primary" onclick="saveSettings()">Save Settings</button>
</div>

<p class="section-title">OTA Firmware Update</p>
<div class="card">
  <label>Upload .bin firmware file</label>
  <input type="file" id="ota-file" accept=".bin" style="margin-bottom:0.5rem;font-size:0.8rem;color:#8b949e">
  <div style="display:flex;align-items:center;gap:0.75rem">
    <button class="btn btn-primary" onclick="uploadOTA()" id="ota-btn">Upload &amp; Flash</button>
    <span id="ota-status" style="font-size:0.8rem;color:#8b949e"></span>
  </div>
  <div style="margin-top:0.5rem;height:6px;border-radius:3px;background:#21262d;overflow:hidden;display:none" id="ota-bar-wrap">
    <div id="ota-bar" style="height:100%;width:0%;background:#238636;border-radius:3px;transition:width 0.3s"></div>
  </div>
</div>

<p class="section-title">Actions</p>
<div class="card">
  <div class="actions-grid">
    <button class="btn" onclick="doAction('upload')">Force Upload</button>
    <button class="btn" onclick="doAction('reboot')">Reboot Device</button>
    <button class="btn btn-danger" onclick="if(confirm('Clear all buffered readings?'))doAction('clear')">Clear Buffer</button>
  </div>
</div>

</main>
<div class="toast" id="toast"></div>

<script>
var wifiNetworks = [];
var bleDevices = [];

function toast(msg, err){
  var t=document.getElementById('toast');
  t.textContent=msg; t.className='toast'+(err?' error':''); t.style.display='block';
  setTimeout(function(){t.style.display='none';},2500);
}

function renderWifi(){
  var el=document.getElementById('wifi-list');
  el.innerHTML='';
  wifiNetworks.forEach(function(w,i){
    var d=document.createElement('div'); d.className='wifi-entry card';
    d.innerHTML='<span>'+w.ssid+'</span><button class="btn btn-sm btn-danger" onclick="removeWifi('+i+')">Remove</button>';
    el.appendChild(d);
  });
}

function addWifi(){
  var ssid=document.getElementById('new-ssid').value.trim();
  var pass=document.getElementById('new-pass').value;
  if(!ssid){toast('SSID required',true);return;}
  if(wifiNetworks.length>=4){toast('Max 4 networks',true);return;}
  wifiNetworks.push({ssid:ssid,password:pass});
  document.getElementById('new-ssid').value='';
  document.getElementById('new-pass').value='';
  renderWifi();
}

function removeWifi(i){
  wifiNetworks.splice(i,1);
  renderWifi();
}

function renderBle(){
  var el=document.getElementById('ble-list');
  el.innerHTML='';
  bleDevices.forEach(function(mac,i){
    var d=document.createElement('div'); d.className='wifi-entry card';
    d.innerHTML='<span>'+mac+'</span><button class="btn btn-sm btn-danger" onclick="removeBle('+i+')">Remove</button>';
    el.appendChild(d);
  });
}

function removeBle(i){
  bleDevices.splice(i,1);
  renderBle();
}

function addBleFromScan(mac){
  mac=mac.toUpperCase();
  if(bleDevices.length>=4){toast('Max 4 BLE devices',true);return;}
  if(bleDevices.indexOf(mac)>=0){toast('Device already added',true);return;}
  bleDevices.push(mac);
  renderBle();
  toast('Added '+mac);
  renderScanResults(lastScanResults);
}

var lastScanResults=[];

function renderScanResults(devices){
  lastScanResults=devices;
  var el=document.getElementById('ble-scan-results');
  if(!devices||devices.length===0){el.innerHTML='<div style="font-size:0.8rem;color:#8b949e;padding:0.25rem 0">No RadiaCode devices found.</div>';return;}
  el.innerHTML='';
  devices.forEach(function(d){
    var already=bleDevices.indexOf(d.mac.toUpperCase())>=0;
    var div=document.createElement('div');
    div.style.cssText='display:flex;align-items:center;gap:0.5rem;padding:0.35rem 0;border-bottom:1px solid #21262d';
    var rssiColor=d.rssi>-60?'#3fb950':d.rssi>-80?'#d29922':'#f85149';
    div.innerHTML='<span style="flex:1;font-size:0.875rem">'+d.name+'</span>'
      +'<span style="font-size:0.75rem;color:#8b949e">'+d.mac+'</span>'
      +'<span style="font-size:0.75rem;color:'+rssiColor+'">'+d.rssi+' dBm</span>'
      +(already?'<span class="btn btn-sm" style="opacity:0.5;cursor:default">Added</span>'
      :'<button class="btn btn-sm btn-primary" onclick="addBleFromScan(\''+d.mac+'\')">Add</button>');
    el.appendChild(div);
  });
}

function scanBle(){
  var btn=document.getElementById('ble-scan-btn');
  var stat=document.getElementById('ble-scan-status');
  btn.disabled=true;btn.textContent='Scanning...';
  stat.textContent='Scanning for ~5 seconds...';
  document.getElementById('ble-scan-results').innerHTML='';
  fetch('/api/ble/scan',{method:'POST'})
    .then(function(r){return r.json();})
    .then(function(d){
      btn.disabled=false;btn.textContent='Scan for RadiaCode';
      stat.textContent=d.devices.length+' device(s) found';
      renderScanResults(d.devices);
    })
    .catch(function(){
      btn.disabled=false;btn.textContent='Scan for RadiaCode';
      stat.textContent='';
      toast('BLE scan failed',true);
    });
}

function loadSettings(){
  fetch('/api/settings').then(function(r){return r.json();}).then(function(d){
    wifiNetworks=d.wifi||[];
    renderWifi();
    bleDevices=d.ble_devices||[];
    renderBle();
    document.getElementById('cfg-token').value=d.token||'';
    document.getElementById('cfg-url').value=d.upload_url||'';
    document.getElementById('cfg-device-id').value=d.device_id||'';
    document.getElementById('cfg-name').value=d.device_name||'';
    document.getElementById('cfg-interval').value=d.reading_interval_ms||2000;
    document.getElementById('cfg-ap-pass').value=d.ap_password||'';
    document.getElementById('cfg-google-key').value=d.google_api_key||'';
  }).catch(function(){toast('Failed to load settings',true);});
}

function saveSettings(){
  var body={
    wifi:wifiNetworks,
    token:document.getElementById('cfg-token').value,
    upload_url:document.getElementById('cfg-url').value,
    device_id:document.getElementById('cfg-device-id').value,
    device_name:document.getElementById('cfg-name').value,
    reading_interval_ms:parseInt(document.getElementById('cfg-interval').value)||2000,
    ap_password:document.getElementById('cfg-ap-pass').value,
    google_api_key:document.getElementById('cfg-google-key').value,
    ble_devices:bleDevices
  };
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();})
    .then(function(d){toast(d.success?'Settings saved':'Save failed: '+(d.error||'unknown'),!d.success);})
    .catch(function(){toast('Save failed',true);});
}

function doAction(action){
  fetch('/api/actions/'+action,{method:'POST'})
    .then(function(r){return r.json();})
    .then(function(d){toast(d.message||action+' done',!d.success);})
    .catch(function(){toast(action+' failed',true);});
}

function uploadOTA(){
  var file=document.getElementById('ota-file').files[0];
  if(!file){toast('Select a .bin file first',true);return;}
  if(!file.name.endsWith('.bin')){toast('File must be a .bin firmware',true);return;}
  var btn=document.getElementById('ota-btn');
  btn.disabled=true;btn.textContent='Uploading...';
  var wrap=document.getElementById('ota-bar-wrap');wrap.style.display='block';
  var bar=document.getElementById('ota-bar');
  var stat=document.getElementById('ota-status');
  stat.textContent='Uploading '+Math.round(file.size/1024)+'KB...';
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/ota');
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);bar.style.width=p+'%';stat.textContent=p+'%';}
  };
  xhr.onload=function(){
    try{var r=JSON.parse(xhr.responseText);
      if(r.success){stat.textContent='Rebooting...';bar.style.background='#3fb950';toast('Update successful, rebooting...');setTimeout(function(){location.reload();},5000);}
      else{stat.textContent='Failed: '+(r.error||'unknown');bar.style.background='#f85149';btn.disabled=false;btn.textContent='Upload & Flash';toast('Update failed',true);}
    }catch(e){stat.textContent='Error';btn.disabled=false;btn.textContent='Upload & Flash';}
  };
  xhr.onerror=function(){stat.textContent='Network error';btn.disabled=false;btn.textContent='Upload & Flash';toast('Upload failed',true);};
  var fd=new FormData();fd.append('firmware',file,file.name);
  xhr.send(fd);
}

loadSettings();

// Status popup — hover + tap
var popup=document.getElementById('status-popup'),ptimer=null,htimer=null;
var ns=document.querySelector('.nav-status');
function fetchPopup(){
  popup.innerHTML='<div style="color:#8b949e">Loading...</div>';
  popup.style.display='block';
  clearTimeout(ptimer);
  ptimer=setTimeout(function(){popup.style.display='none';},5000);
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    var h='';
    h+='<div class="sr"><span class="sl">WiFi</span><span class="'+(d.wifi_connected?'st-ok':'st-err')+'" style="font-weight:600">'+(d.wifi_connected?(d.wifi_ssid||'Connected'):'Disconnected')+'</span></div>';
    if(d.wifi_sta_ip&&d.wifi_connected)h+='<div class="sr"><span class="sl">IP</span><span style="color:#58a6ff">'+d.wifi_sta_ip+'</span></div>';
    h+='<div class="sr"><span class="sl">GPS</span><span class="'+(d.gps_fix?'st-ok':'st-warn')+'" style="font-weight:600">'+(d.gps_fix?d.gps_sats+' sats':'No Fix')+'</span></div>';
    h+='<div class="sr"><span class="sl">USB</span><span class="'+(d.usb_connected?'st-ok':'st-err')+'" style="font-weight:600">'+(d.usb_connected?'Connected':'Disconnected')+'</span></div>';
    h+='<div class="sr"><span class="sl">Buffer</span><span style="color:#c9d1d9;font-weight:600">'+d.buffer_pending+' pending</span></div>';
    h+='<div class="sr"><span class="sl">Upload</span><span style="color:#c9d1d9">'+(d.last_upload||'Never')+'</span></div>';
    document.getElementById('nav-wifi').className='dot '+(d.wifi_connected?'dot-green':'dot-red');
    document.getElementById('nav-gps').className='dot '+(d.gps_fix?'dot-green':'dot-red');
    document.getElementById('nav-usb').className='dot '+(d.usb_connected?'dot-green':'dot-red');
    popup.innerHTML=h;
  }).catch(function(){popup.innerHTML='<div class="st-err">Failed to load</div>';});
}
function hidePopup(){popup.style.display='none';clearTimeout(ptimer);}
ns.addEventListener('click',function(e){e.stopPropagation();if(popup.style.display==='block'){hidePopup();return;}fetchPopup();});
ns.addEventListener('mouseenter',function(){clearTimeout(htimer);fetchPopup();});
ns.addEventListener('mouseleave',function(){htimer=setTimeout(hidePopup,400);});
popup.addEventListener('mouseenter',function(){clearTimeout(htimer);clearTimeout(ptimer);});
popup.addEventListener('mouseleave',function(){hidePopup();});
document.addEventListener('click',function(){hidePopup();});
</script>
</body>
</html>)rawhtml";

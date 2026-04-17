#pragma once
#include <pgmspace.h>

// Debug console page — WebSocket log viewer with module/level filtering
const char DEBUG_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RadiaLog — Debug Console</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0d1117;color:#c9d1d9;font-size:16px;line-height:1.5}
a{color:#58a6ff;text-decoration:none}
a:hover{text-decoration:underline}
nav{background:#161b22;border-bottom:1px solid #30363d;padding:0 1rem;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.1rem;color:#f0f6fc;flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nav-links{display:flex;gap:0.25rem;margin:0 0.75rem}
.nav-links a{color:#8b949e;padding:0.35rem 0.6rem;border-radius:6px;font-size:0.875rem;white-space:nowrap}
.nav-links a:hover,.nav-links a.active{color:#f0f6fc;background:#21262d;text-decoration:none}
.nav-status{display:flex;align-items:center;gap:0.5rem;flex-shrink:0}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;flex-shrink:0}
.dot-green{background:#3fb950}.dot-red{background:#f85149}.dot-gray{background:#484f58}.dot-yellow{background:#d29922}
main{max-width:900px;margin:0 auto;padding:1rem}
.filters{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.75rem}
.filter-label{display:inline-flex;align-items:center;gap:0.3rem;font-size:0.8rem;cursor:pointer;background:#21262d;border:1px solid #30363d;border-radius:20px;padding:0.2rem 0.6rem;user-select:none}
.filter-label input{accent-color:#58a6ff}
select{padding:0.3rem 0.5rem;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:0.8rem;outline:none}
select:focus{border-color:#58a6ff}
.btn{display:inline-flex;align-items:center;gap:0.35rem;padding:0.35rem 0.7rem;border-radius:6px;border:1px solid transparent;font-size:0.8rem;font-weight:500;cursor:pointer;transition:background .15s}
.btn-secondary{background:#21262d;border-color:#30363d;color:#c9d1d9}
.btn-secondary:hover{background:#30363d}
.btn-danger{background:#b62324;border-color:#f85149;color:#fff}
.btn-danger:hover{background:#da3633}
.log-console{background:#0d1117;border:1px solid #30363d;border-radius:6px;height:500px;overflow-y:auto;font-family:'Courier New',Courier,monospace;font-size:0.8rem;padding:0.5rem}
.log-entry{padding:1px 0;white-space:pre-wrap;word-break:break-all;border-bottom:1px solid rgba(255,255,255,.03)}
.level-ERROR{color:#f85149}
.level-WARN{color:#d29922}
.level-INFO{color:#c9d1d9}
.level-DEBUG{color:#484f58}
.reconnecting{background:#d2992233;border:1px solid #d29922;border-radius:6px;padding:0.5rem 1rem;margin-bottom:0.75rem;color:#d29922;font-size:0.875rem;text-align:center}
.hidden{display:none!important}
.controls-row{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.75rem;justify-content:space-between}
.controls-left{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center}
.controls-right{display:flex;gap:0.5rem;align-items:center}
.entry-count{font-size:0.75rem;color:#8b949e}
.status-popup{display:none;position:fixed;top:56px;right:1rem;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:0.75rem 1rem;z-index:200;min-width:200px;box-shadow:0 4px 12px rgba(0,0,0,0.4);font-size:0.8rem}
.status-popup .sr{display:flex;justify-content:space-between;align-items:center;padding:0.25rem 0}
.status-popup .sl{color:#8b949e}.st-ok{color:#3fb950}.st-err{color:#f85149}.st-warn{color:#d29922}
@media(max-width:480px){.nav-links a{padding:0.3rem 0.4rem;font-size:0.8rem}.log-console{height:350px}}
</style>
</head>
<body>
<nav>
  <span class="nav-brand">&#9762; RadiaLog</span>
  <div class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/debug" class="active">Debug</a>
    <a href="/settings">Settings</a>
    <a href="/data">Data</a>
    <a href="/self-test">Self-Test</a>
  </div>
  <div class="nav-status" style="cursor:pointer">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="USB"></span>
  </div>
</nav>
<div class="status-popup" id="status-popup"></div>
<main>
  <div class="reconnecting hidden" id="reconnect-msg">&#9888; WebSocket disconnected — reconnecting...</div>

  <div class="controls-row">
    <div class="controls-left">
      <div class="filters">
        <label class="filter-label"><input type="checkbox" id="filter-USB" checked> USB</label>
        <label class="filter-label"><input type="checkbox" id="filter-GPS" checked> GPS</label>
        <label class="filter-label"><input type="checkbox" id="filter-WIFI" checked> WIFI</label>
        <label class="filter-label"><input type="checkbox" id="filter-UPLOAD" checked> UPLOAD</label>
        <label class="filter-label"><input type="checkbox" id="filter-BUFFER" checked> BUFFER</label>
        <label class="filter-label"><input type="checkbox" id="filter-BLE" checked> BLE</label>
      </div>
      <select id="level-filter">
        <option value="DEBUG">DEBUG</option>
        <option value="INFO" selected>INFO</option>
        <option value="WARN">WARN</option>
        <option value="ERROR">ERROR</option>
      </select>
    </div>
    <div class="controls-right">
      <span class="entry-count" id="entry-count">0 entries</span>
      <button class="btn btn-danger" id="btn-clear">Clear</button>
    </div>
  </div>

  <div class="log-console" id="log-div"></div>
</main>

<script>
(function(){
  var logDiv = document.getElementById('log-div');
  var reconnectMsg = document.getElementById('reconnect-msg');
  var entryCount = document.getElementById('entry-count');
  var levelSelect = document.getElementById('level-filter');
  var btnClear = document.getElementById('btn-clear');
  var wsDot = document.getElementById('nav-ws');

  var modules = ['USB','GPS','WIFI','UPLOAD','BUFFER','BLE'];
  var levelOrder = {DEBUG:0, INFO:1, WARN:2, ERROR:3};
  var ws = null;
  var autoScroll = true;
  var reconnectTimer = null;
  var totalEntries = 0;

  // Track scroll position to determine auto-scroll
  logDiv.addEventListener('scroll', function(){
    var atBottom = logDiv.scrollHeight - logDiv.clientHeight - logDiv.scrollTop < 30;
    autoScroll = atBottom;
  });

  function getActiveModules(){
    var active = {};
    for(var i=0; i<modules.length; i++){
      var cb = document.getElementById('filter-'+modules[i]);
      if(cb) active[modules[i]] = cb.checked;
    }
    return active;
  }

  function getMinLevel(){
    return levelOrder[levelSelect.value] || 0;
  }

  function shouldShow(entry){
    var activeModules = getActiveModules();
    var mod = entry.getAttribute('data-module');
    var lvl = entry.getAttribute('data-level');
    if(mod && activeModules[mod] === false) return false;
    if(lvl && (levelOrder[lvl]||0) < getMinLevel()) return false;
    return true;
  }

  function refilter(){
    var entries = logDiv.getElementsByClassName('log-entry');
    var visible = 0;
    for(var i=0; i<entries.length; i++){
      if(shouldShow(entries[i])){
        entries[i].style.display = '';
        visible++;
      } else {
        entries[i].style.display = 'none';
      }
    }
    entryCount.textContent = visible + '/' + totalEntries + ' entries';
  }

  // Bind filter changes
  for(var i=0; i<modules.length; i++){
    var cb = document.getElementById('filter-'+modules[i]);
    if(cb) cb.addEventListener('change', refilter);
  }
  levelSelect.addEventListener('change', refilter);

  // Clear button
  btnClear.addEventListener('click', function(){
    logDiv.innerHTML = '';
    totalEntries = 0;
    entryCount.textContent = '0 entries';
  });

  function formatTs(ts){
    if(!ts) return '00:00:00';
    var s = Math.floor(ts/1000);
    var h = Math.floor(s/3600) % 24;
    var m = Math.floor(s/60) % 60;
    var sec = s % 60;
    return (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(sec<10?'0':'')+sec;
  }

  function addEntry(data){
    var div = document.createElement('div');
    div.className = 'log-entry level-' + (data.level||'INFO');
    div.setAttribute('data-module', data.module||'');
    div.setAttribute('data-level', data.level||'INFO');
    div.textContent = '[' + formatTs(data.ts) + '] [' + (data.module||'?') + '] ' + (data.msg||'');
    logDiv.appendChild(div);
    totalEntries++;

    if(!shouldShow(div)){
      div.style.display = 'none';
    }

    // Update count
    var entries = logDiv.getElementsByClassName('log-entry');
    var visible = 0;
    for(var j=0; j<entries.length; j++){
      if(entries[j].style.display !== 'none') visible++;
    }
    entryCount.textContent = visible + '/' + totalEntries + ' entries';

    if(autoScroll){
      logDiv.scrollTop = logDiv.scrollHeight;
    }
  }

  function connect(){
    if(ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) return;

    ws = new WebSocket('ws://'+location.host+'/ws/debug');

    ws.onopen = function(){
      reconnectMsg.classList.add('hidden');
      wsDot.className = 'dot dot-green';
      if(reconnectTimer){ clearTimeout(reconnectTimer); reconnectTimer = null; }
    };

    ws.onmessage = function(evt){
      try{
        var data = JSON.parse(evt.data);
        addEntry(data);
      } catch(e){}
    };

    ws.onclose = function(){
      wsDot.className = 'dot dot-red';
      reconnectMsg.classList.remove('hidden');
      reconnectTimer = setTimeout(connect, 3000);
    };

    ws.onerror = function(){
      ws.close();
    };
  }

  connect();

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
      var el;el=document.getElementById('nav-wifi');if(el)el.className='dot '+(d.wifi_connected?'dot-green':'dot-red');
      el=document.getElementById('nav-gps');if(el)el.className='dot '+(d.gps_fix?'dot-green':'dot-red');
      el=document.getElementById('nav-usb');if(el)el.className='dot '+(d.usb_connected?'dot-green':'dot-red');
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
})();
</script>
<script src="/templates.js"></script>
</body>
</html>)rawhtml";

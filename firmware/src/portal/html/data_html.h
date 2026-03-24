#pragma once
#include <pgmspace.h>

// Data view page — dose rate chart + recent readings table + CSV export
const char DATA_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RadiaLog — Data</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0d1117;color:#c9d1d9;font-size:16px;line-height:1.5}
a{color:#58a6ff;text-decoration:none}a:hover{text-decoration:underline}
nav{background:#161b22;border-bottom:1px solid #30363d;padding:0 1rem;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.1rem;color:#f0f6fc;flex:1}
.nav-links{display:flex;gap:0.25rem;margin:0 0.75rem}
.nav-links a{color:#8b949e;padding:0.35rem 0.6rem;border-radius:6px;font-size:0.875rem;white-space:nowrap}
.nav-links a:hover,.nav-links a.active{color:#f0f6fc;background:#21262d;text-decoration:none}
main{max-width:900px;margin:0 auto;padding:1rem}
.toolbar{display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem}
.toolbar-info{font-size:0.8rem;color:#8b949e}
.btn{display:inline-block;padding:0.4rem 0.8rem;border-radius:6px;border:1px solid #30363d;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:0.8rem;text-decoration:none}
.btn:hover{background:#30363d}
.chart-card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:0.75rem;margin-bottom:1rem}
.chart-title{font-size:0.75rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin-bottom:0.25rem}
.chart-labels{display:flex;justify-content:space-between;font-size:0.65rem;color:#484f58;margin-top:0.15rem}
table{width:100%;border-collapse:collapse;font-size:0.8rem}
thead{position:sticky;top:52px;z-index:10}
th{background:#161b22;border:1px solid #30363d;padding:0.4rem 0.5rem;text-align:left;color:#8b949e;font-weight:600;white-space:nowrap}
td{border:1px solid #21262d;padding:0.3rem 0.5rem;white-space:nowrap}
tr:hover td{background:#161b22}
.empty{text-align:center;color:#484f58;padding:2rem}
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
    <a href="/settings">Settings</a>
    <a href="/data" class="active">Data</a>
  </div>
  <div class="nav-status" style="cursor:pointer">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="USB"></span>
  </div>
</nav>
<div class="status-popup" id="status-popup"></div>
<main>
  <div class="chart-card">
    <div class="chart-title">Dose Rate (&micro;Sv/h)</div>
    <svg id="dose-chart" width="100%" height="120" preserveAspectRatio="none" viewBox="0 0 800 120">
      <rect width="800" height="120" fill="none"/>
      <polyline id="chart-line" fill="none" stroke="#3fb950" stroke-width="1.5" stroke-linejoin="round"/>
      <polyline id="chart-area" fill="url(#areaGrad)" stroke="none"/>
      <defs><linearGradient id="areaGrad" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%" stop-color="#3fb950" stop-opacity="0.3"/>
        <stop offset="100%" stop-color="#3fb950" stop-opacity="0.02"/>
      </linearGradient></defs>
    </svg>
    <div class="chart-labels">
      <span id="chart-min">—</span>
      <span id="chart-max">—</span>
    </div>
  </div>

  <div class="toolbar">
    <div class="toolbar-info">
      Showing <span id="row-count">0</span> most recent readings
      <button class="btn" style="margin-left:0.5rem" onclick="loadData()">Refresh</button>
    </div>
    <a class="btn" href="/api/readings/csv" download="radialog_readings.csv">Export CSV</a>
  </div>
  <div style="overflow-x:auto">
    <table>
      <thead>
        <tr>
          <th>#</th><th>Time</th><th>Dose (uSv/h)</th><th>CPS</th>
          <th>Lat</th><th>Lon</th><th>Alt (m)</th>
          <th>Speed (kph)</th><th>Heading</th><th>Acc (m)</th>
        </tr>
      </thead>
      <tbody id="data-body">
        <tr><td colspan="10" class="empty">Loading...</td></tr>
      </tbody>
    </table>
  </div>
</main>
<script>
function drawChart(readings){
  var line=document.getElementById('chart-line');
  var area=document.getElementById('chart-area');
  if(!readings||readings.length<2){line.setAttribute('points','');area.setAttribute('points','');return;}

  var doses=readings.map(function(r){return r.dose_rate||0;});
  var maxD=Math.max.apply(null,doses)||0.001;
  var minD=Math.min.apply(null,doses);

  document.getElementById('chart-min').textContent='Min: '+minD.toFixed(4);
  document.getElementById('chart-max').textContent='Max: '+maxD.toFixed(4);

  var w=800,h=110,pad=5;
  var range=maxD-minD||0.001;
  var pts=[];
  for(var i=0;i<readings.length;i++){
    var x=(i/(readings.length-1))*w;
    var y=h-pad-((doses[i]-minD)/range)*(h-pad*2);
    pts.push(x.toFixed(1)+','+y.toFixed(1));
  }
  line.setAttribute('points',pts.join(' '));
  area.setAttribute('points','0,'+h+' '+pts.join(' ')+' '+w+','+h);
}

function loadData(){
  fetch('/api/readings')
    .then(function(r){return r.json();})
    .then(function(d){
      var body=document.getElementById('data-body');
      var readings=d.readings||[];
      document.getElementById('row-count').textContent=readings.length;

      drawChart(readings);

      if(readings.length===0){
        body.innerHTML='<tr><td colspan="10" class="empty">No readings in buffer</td></tr>';
        return;
      }
      var html='';
      readings.forEach(function(r,i){
        html+='<tr>';
        html+='<td>'+(r.id!=null?r.id:i)+'</td>';
        html+='<td>'+(r.timestamp?new Date(r.timestamp*1000).toLocaleTimeString():'—')+'</td>';
        html+='<td>'+(r.dose_rate!=null?r.dose_rate.toFixed(4):'—')+'</td>';
        html+='<td>'+(r.count_rate!=null?r.count_rate.toFixed(1):'—')+'</td>';
        html+='<td>'+(r.lat!=null?r.lat.toFixed(5):'—')+'</td>';
        html+='<td>'+(r.lon!=null?r.lon.toFixed(5):'—')+'</td>';
        html+='<td>'+(r.altitude!=null?r.altitude.toFixed(1):'—')+'</td>';
        html+='<td>'+(r.speed_kph!=null?r.speed_kph.toFixed(1):'—')+'</td>';
        html+='<td>'+(r.heading!=null?r.heading.toFixed(0):'—')+'</td>';
        html+='<td>'+(r.accuracy!=null?r.accuracy.toFixed(1):'—')+'</td>';
        html+='</tr>';
      });
      body.innerHTML=html;
    })
    .catch(function(){
      document.getElementById('data-body').innerHTML='<tr><td colspan="10" class="empty">Failed to load readings</td></tr>';
    });
}
loadData();

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

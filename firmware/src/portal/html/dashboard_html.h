#pragma once
#include <pgmspace.h>

// Dashboard page — auto-refreshes every 2s from /api/status
const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RadiaLog — Dashboard</title>
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
main{max-width:720px;margin:0 auto;padding:1rem}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:1rem;margin-bottom:1rem}
.card-title{font-size:0.8rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin-bottom:0.5rem}
.card-value{font-size:1.6rem;font-weight:700;color:#f0f6fc}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:0.75rem;margin-bottom:1rem}
.card-sm{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:0.75rem}
.card-sm .card-title{font-size:0.75rem;margin-bottom:0.35rem}
.card-sm .card-value{font-size:1.25rem}
.detail{font-size:0.8rem;color:#8b949e;margin-top:0.25rem}
.status-ok{color:#3fb950}.status-err{color:#f85149}.status-warn{color:#d29922}
.alert-conn{display:none;background:#f8514922;border:1px solid #f85149;border-radius:6px;padding:0.6rem 1rem;margin-bottom:1rem;color:#f85149;font-weight:600}
.section-title{font-size:0.875rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin:1rem 0 0.5rem}
.batt-bar{height:6px;border-radius:3px;background:#21262d;margin-top:0.35rem;overflow:hidden}
.batt-fill{height:100%;border-radius:3px;transition:width 0.5s}
.version-info{font-size:0.75rem;color:#484f58;text-align:center;margin-top:1rem}
@media(max-width:480px){.nav-links a{padding:0.3rem 0.4rem;font-size:0.8rem}.card-value{font-size:1.25rem}}
</style>
</head>
<body>
<nav>
  <span class="nav-brand">&#9762; RadiaLog</span>
  <div class="nav-links">
    <a href="/" class="active">Dashboard</a>
    <a href="/debug">Debug</a>
    <a href="/settings">Settings</a>
    <a href="/data">Data</a>
  </div>
  <div class="nav-status">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="USB"></span>
  </div>
</nav>
<main>
  <div class="alert-conn" id="conn-lost">&#9888; Connection lost — device unreachable</div>

  <p class="section-title">Radiation</p>
  <div class="stats-grid">
    <div class="card-sm">
      <div class="card-title">Dose Rate</div>
      <div class="card-value" id="dose-rate">—</div>
      <div class="detail">&micro;Sv/h</div>
    </div>
    <div class="card-sm">
      <div class="card-title">Count Rate</div>
      <div class="card-value" id="count-rate">—</div>
      <div class="detail">CPS</div>
    </div>
  </div>

  <p class="section-title">Location</p>
  <div class="card">
    <div class="card-title">GPS Fix</div>
    <div class="card-value" id="gps-fix">—</div>
    <div class="detail">
      Lat: <span id="gps-lat">—</span> &nbsp;
      Lon: <span id="gps-lon">—</span> &nbsp;
      Sats: <span id="gps-sats">—</span>
    </div>
  </div>

  <p class="section-title">Connectivity</p>
  <div class="stats-grid">
    <div class="card-sm">
      <div class="card-title">WiFi STA</div>
      <div class="card-value" id="wifi-status">—</div>
      <div class="detail">RSSI: <span id="wifi-rssi">—</span> dBm</div>
      <div class="detail" id="wifi-ssid" style="word-break:break-all"></div>
    </div>
    <div class="card-sm">
      <div class="card-title">USB</div>
      <div class="card-value" id="usb-status">—</div>
    </div>
  </div>

  <p class="section-title">Buffer &amp; Upload</p>
  <div class="stats-grid">
    <div class="card-sm">
      <div class="card-title">Pending Upload</div>
      <div class="card-value" id="buffer-pending">—</div>
      <div class="detail">of <span id="buffer-total">—</span> in buffer</div>
    </div>
    <div class="card-sm">
      <div class="card-title">Last Upload</div>
      <div class="card-value" style="font-size:1rem" id="last-upload">—</div>
    </div>
  </div>

  <p class="section-title">Power</p>
  <div class="card">
    <div class="stats-grid" style="margin-bottom:0">
      <div class="card-sm">
        <div class="card-title">Battery</div>
        <div class="card-value" id="batt-pct">—</div>
        <div class="detail"><span id="batt-volts">—</span>V</div>
        <div class="batt-bar"><div class="batt-fill" id="batt-fill" style="width:0%;background:#3fb950"></div></div>
      </div>
      <div class="card-sm">
        <div class="card-title">Uptime</div>
        <div class="card-value" style="font-size:1rem" id="uptime">—</div>
      </div>
      <div class="card-sm">
        <div class="card-title">Time Sync</div>
        <div class="card-value" style="font-size:1rem" id="time-sync">—</div>
      </div>
    </div>
  </div>

  <p class="section-title">RadiaMaps Account</p>
  <div class="card">
    <div class="stats-grid" style="margin-bottom:0">
      <div class="card-sm">
        <div class="card-title">Username</div>
        <div class="card-value" style="font-size:1rem" id="rm-username">—</div>
      </div>
      <div class="card-sm">
        <div class="card-title">Subscription</div>
        <div class="card-value" style="font-size:1rem" id="rm-subscription">—</div>
      </div>
      <div class="card-sm">
        <div class="card-title">Lifetime Readings</div>
        <div class="card-value" id="rm-lifetime">—</div>
      </div>
    </div>
  </div>

  <div class="version-info" id="version-info"></div>
</main>

<script>
(function(){
  var connLost = document.getElementById('conn-lost');
  function setText(id, val){ var el=document.getElementById(id); if(el) el.textContent=val; }
  function dotColor(id, ok){ var el=document.getElementById(id); if(!el)return; el.className='dot '+(ok?'dot-green':'dot-red'); }

  function fmtUptime(s){
    if(s<60) return s+'s';
    if(s<3600) return Math.floor(s/60)+'m '+s%60+'s';
    var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);
    if(h<24) return h+'h '+m+'m';
    return Math.floor(h/24)+'d '+h%24+'h';
  }

  function refresh(){
    fetch('/api/status')
      .then(function(r){ if(!r.ok) throw new Error('bad'); return r.json(); })
      .then(function(d){
        connLost.style.display='none';

        // Radiation
        setText('dose-rate', typeof d.dose_rate==='number' ? d.dose_rate.toFixed(3) : '—');
        setText('count-rate', typeof d.count_rate==='number' ? d.count_rate.toFixed(1) : '—');

        // GPS
        var gpsOk = d.gps_fix===true||d.gps_fix===1;
        setText('gps-fix', gpsOk?'Fixed':'No Fix');
        var gpsEl=document.getElementById('gps-fix');
        if(gpsEl) gpsEl.className='card-value '+(gpsOk?'status-ok':'status-err');
        setText('gps-lat', gpsOk&&d.gps_lat!=null?d.gps_lat.toFixed(5):'—');
        setText('gps-lon', gpsOk&&d.gps_lon!=null?d.gps_lon.toFixed(5):'—');
        setText('gps-sats', d.gps_sats!=null?d.gps_sats:'—');
        dotColor('nav-gps', gpsOk);

        // WiFi
        var wifiOk=d.wifi_connected===true||d.wifi_connected===1;
        setText('wifi-status', wifiOk?'Connected':'Disconnected');
        var wifiEl=document.getElementById('wifi-status');
        if(wifiEl) wifiEl.className='card-value '+(wifiOk?'status-ok':'status-err');
        setText('wifi-rssi', d.wifi_rssi!=null?d.wifi_rssi:'—');
        setText('wifi-ssid', d.wifi_ssid||'');
        dotColor('nav-wifi', wifiOk);

        // USB
        var usbOk=d.usb_connected===true||d.usb_connected===1;
        setText('usb-status', usbOk?'Connected':'Disconnected');
        var usbEl=document.getElementById('usb-status');
        if(usbEl) usbEl.className='card-value '+(usbOk?'status-ok':'status-err');
        dotColor('nav-usb', usbOk);

        // Buffer
        setText('buffer-pending', d.buffer_pending!=null?d.buffer_pending:'—');
        setText('buffer-total', d.buffer_total!=null?d.buffer_total:'—');
        setText('last-upload', d.last_upload||'Never');

        // Battery
        var pct=d.battery_percent!=null?d.battery_percent:0;
        var volts=d.battery_voltage!=null?d.battery_voltage:0;
        setText('batt-pct', volts>2?pct+'%':'N/A');
        setText('batt-volts', volts>2?volts.toFixed(2):'—');
        var fill=document.getElementById('batt-fill');
        if(fill){
          fill.style.width=pct+'%';
          fill.style.background=pct>20?'#3fb950':(pct>5?'#d29922':'#f85149');
        }

        // Uptime + time
        setText('uptime', d.uptime!=null?fmtUptime(d.uptime):'—');
        var synced=d.time_synced===true||d.time_synced===1;
        setText('time-sync', synced?'NTP OK':'Not synced');
        var tsEl=document.getElementById('time-sync');
        if(tsEl) tsEl.className='card-value '+(synced?'status-ok':'status-warn');

        // RadiaMaps
        setText('rm-username', d.rm_username||'—');
        setText('rm-subscription', d.rm_subscription||'—');
        setText('rm-lifetime', d.rm_lifetime_readings!=null?d.rm_lifetime_readings:'—');

        // Version
        var vi=document.getElementById('version-info');
        if(vi&&d.fw_version) vi.textContent='RadiaLog v'+d.fw_version;
      })
      .catch(function(){
        connLost.style.display='block';
        dotColor('nav-wifi','');dotColor('nav-gps','');dotColor('nav-usb','');
      });
  }

  refresh();
  setInterval(refresh, 2000);
})();
</script>
</body>
</html>)rawhtml";

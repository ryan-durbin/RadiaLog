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

:root{
--primary:#FFE030;--primary-dim:rgba(255,224,48,0.11);--primary-glow:rgba(255,224,48,0.18);
--secondary:#F08000;--secondary-dim:rgba(240,128,0,0.1);
--green:#4ade80;--green-dim:rgba(74,222,128,0.1);
--danger:#ff6b6b;--danger-dim:rgba(255,107,107,0.1);
--cyan:#67e8f9;--warn:#FFE030;--warn-dim:rgba(255,224,48,0.1);
--bg-base:#0b0b0d;--bg-surface:#131315;--bg-elevated:#1a1a1e;
--border-subtle:rgba(255,224,48,0.09);--border-medium:rgba(255,224,48,0.18);
--text-primary:#ecedf0;--text-secondary:rgba(236,237,240,0.68);--text-tertiary:rgba(236,237,240,0.38);
--text-on-primary:#0b0b0d;
--radius-md:7px;--radius-sm:4px;
--shadow-glow:0 0 20px -6px rgba(255,224,48,0.15);
--font-display:'Rajdhani',system-ui,sans-serif;--font-mono:'JetBrains Mono','Fira Code','Courier New',monospace;
}

@import url('https://fonts.googleapis.com/css2?family=Rajdhani:wght@500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');

body{font-family:system-ui,-apple-system,sans-serif;background:var(--bg-base);color:var(--text-primary);font-size:14px;line-height:1.5}

/* Grid background */
body::before{content:'';position:fixed;inset:0;z-index:0;pointer-events:none;
background-image:
linear-gradient(0deg,transparent 24%,rgba(255,224,48,0.012) 25%,rgba(255,224,48,0.012) 26%,transparent 27%,transparent 74%,rgba(255,224,48,0.012) 75%,rgba(255,224,48,0.012) 76%,transparent 77%,transparent),
linear-gradient(90deg,transparent 24%,rgba(255,224,48,0.012) 25%,rgba(255,224,48,0.012) 26%,transparent 27%,transparent 74%,rgba(255,224,48,0.012) 75%,rgba(255,224,48,0.012) 76%,transparent 77%,transparent);
background-size:50px 50px}

a{color:var(--primary);text-decoration:none}
a:hover{text-decoration:underline;text-shadow:0 0 8px var(--primary-glow)}

/* ── NAV ── */
nav{background:rgba(20,18,16,0.95);backdrop-filter:blur(12px);border-bottom:1px solid var(--primary);
padding:0 14px;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.2rem;color:var(--primary);flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;
font-family:var(--font-display);letter-spacing:3px;text-transform:uppercase;text-shadow:0 0 8px var(--primary-glow)}
.nav-links{display:flex;gap:2px;margin:0 0.75rem}
.nav-links a{color:var(--text-tertiary);padding:0.35rem 0.6rem;border-radius:var(--radius-sm);font-size:12px;white-space:nowrap;
font-family:var(--font-display);letter-spacing:0.3px;font-weight:500;transition:all 0.15s}
.nav-links a:hover{color:var(--text-secondary);background:var(--primary-dim);text-decoration:none}
.nav-links a.active{color:var(--primary);border-bottom:2px solid var(--primary);text-decoration:none}
.nav-status{display:flex;align-items:center;gap:6px;flex-shrink:0}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;flex-shrink:0;transition:all 0.3s}
.dot-green{background:var(--green);box-shadow:0 0 6px rgba(74,222,128,0.4)}
.dot-red{background:var(--danger);box-shadow:0 0 6px rgba(255,107,107,0.4)}
.dot-gray{background:rgba(255,255,255,0.15)}
.dot-yellow{background:var(--primary);box-shadow:0 0 6px var(--primary-glow)}

/* ── MAIN ── */
main{max-width:720px;margin:0 auto;padding:1rem;position:relative;z-index:1}

/* ── CARDS ── */
.card{background:var(--bg-surface);border:1px solid var(--border-subtle);border-radius:var(--radius-md);
padding:1rem;margin-bottom:1rem;position:relative;overflow:hidden;
box-shadow:var(--shadow-glow)}
.card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
background:linear-gradient(90deg,var(--primary),var(--secondary));opacity:0.6}
.card-title{font-size:10px;text-transform:uppercase;letter-spacing:1px;color:var(--text-tertiary);margin-bottom:4px;
font-family:var(--font-display)}
.card-value{font-size:1.5rem;font-weight:700;color:var(--text-primary);font-family:var(--font-mono)}

.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px;margin-bottom:1rem}
#rad-conn-grid{grid-template-columns:repeat(4,1fr)}

.card-sm{background:var(--bg-elevated);border:1px solid var(--border-subtle);border-radius:var(--radius-md);
padding:14px;position:relative;overflow:hidden}
.card-sm::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
background:linear-gradient(90deg,var(--primary),var(--secondary));opacity:0.6}
.card-sm .card-title{font-size:10px;margin-bottom:4px}
.card-sm .card-value{font-size:1.25rem}

.detail{font-size:11px;color:var(--text-tertiary);margin-top:3px;font-family:var(--font-mono)}
.status-ok{color:var(--green)!important}.status-err{color:var(--danger)!important}.status-warn{color:var(--warn)!important}

/* ── ALERTS ── */
.alert-conn{display:none;background:var(--danger-dim);border:1px solid rgba(255,107,107,0.3);border-radius:var(--radius-md);
padding:0.6rem 1rem;margin-bottom:1rem;color:var(--danger);font-weight:600;font-family:var(--font-display);letter-spacing:0.3px}
.alert-token{display:none;background:var(--warn-dim);border:1px solid rgba(255,224,48,0.3);border-radius:var(--radius-md);
padding:0.6rem 1rem;margin-bottom:1rem;color:var(--primary);font-weight:600;font-family:var(--font-display);letter-spacing:0.3px}

/* ── SECTION TITLES ── */
.section-title{font-size:11px;font-weight:600;letter-spacing:2px;text-transform:uppercase;color:var(--text-tertiary);
margin:1.5rem 0 0.5rem;padding-bottom:8px;border-bottom:1px solid var(--border-subtle);font-family:var(--font-display)}

/* ── BATTERY BAR ── */
.batt-bar{height:4px;border-radius:2px;background:rgba(255,255,255,0.06);margin-top:0.5rem;overflow:hidden}
.batt-fill{height:100%;border-radius:2px;transition:width 0.4s ease;
background:linear-gradient(90deg,var(--primary),var(--secondary))}

/* ── VERSION ── */
.version-info{font-size:11px;color:var(--text-tertiary);text-align:center;margin-top:1.5rem;
font-family:var(--font-mono);letter-spacing:0.5px}

/* ── STATUS POPUP ── */
.status-popup{display:none;position:fixed;top:56px;right:1rem;background:var(--bg-surface);border:1px solid var(--border-medium);
border-radius:var(--radius-md);padding:0.75rem 1rem;z-index:200;min-width:220px;
box-shadow:0 6px 28px rgba(0,0,0,0.6),0 0 0 1px var(--border-subtle),var(--shadow-glow);font-size:12px}
.status-popup .sr{display:flex;justify-content:space-between;align-items:center;padding:0.3rem 0}
.status-popup .sr+.sr{border-top:1px solid var(--border-subtle)}
.status-popup .sl{color:var(--text-tertiary);font-family:var(--font-display);letter-spacing:0.3px;font-weight:500}
.st-ok{color:var(--green)}.st-err{color:var(--danger)}.st-warn{color:var(--warn)}

/* ── LOCATION CARD ── */
#location-card{background:var(--bg-surface);border:1px solid var(--border-subtle);box-shadow:var(--shadow-glow)}
#location-card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
background:linear-gradient(90deg,var(--primary),var(--secondary));opacity:0.6}
#minimap-crosshair{border-color:var(--primary)!important;box-shadow:0 0 0 3px var(--primary-dim),0 0 8px var(--primary-glow)!important}

@media(max-width:480px){
.nav-links a{padding:0.3rem 0.4rem;font-size:11px}
.card-value{font-size:1.1rem}
.card-sm .card-value{font-size:1rem}
#rad-conn-grid{grid-template-columns:repeat(2,1fr)}
}
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
    <a href="/self-test">Self-Test</a>
  </div>
  <div class="nav-status">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="RadiaCode"></span>
  </div>
</nav>
<div class="status-popup" id="status-popup"></div>
<main>
  <div class="alert-conn" id="conn-lost">&#9888; Connection lost — device unreachable</div>
  <div class="alert-token" id="no-token">&#9888; No device token configured — uploads disabled. <a href="/settings">Configure in Settings</a></div>

  <section id="sect-radiation">
  <p class="section-title">Radiation &amp; Connectivity</p>
  <div class="stats-grid" id="rad-conn-grid">
    <div class="card-sm">
      <div class="card-title">Dose Rate</div>
      <div class="card-value" id="dose-rate">—</div>
      <div class="detail">nSv/h</div>
    </div>
    <div class="card-sm">
      <div class="card-title">Count Rate</div>
      <div class="card-value" id="count-rate">—</div>
      <div class="detail">CPS</div>
    </div>
    <div class="card-sm conn-card">
      <div class="card-title">WiFi STA</div>
      <div class="card-value" id="wifi-status">—</div>
      <div class="detail">RSSI: <span id="wifi-rssi">—</span> dBm</div>
      <div class="detail" id="wifi-ssid" style="word-break:break-all"></div>
    </div>
    <div class="card-sm conn-card">
      <div class="card-title">RadiaCode</div>
      <div class="card-value" id="rc-status">—</div>
      <div class="detail" id="rc-source"></div>
    </div>
  </div>
  </section>

  <section id="sect-location">
  <p class="section-title">Location</p>
  <div class="card" id="location-card" style="position:relative;overflow:hidden;min-height:160px;padding:0">
    <div id="minimap-bg" style="position:absolute;inset:0;background:var(--bg-base);background-size:cover;background-position:center;opacity:0.5;filter:grayscale(0.6) brightness(0.9);transition:opacity 0.8s"></div>
    <div style="position:absolute;inset:0;background:linear-gradient(135deg,rgba(11,11,13,0.85) 0%,rgba(11,11,13,0.5) 50%,rgba(11,11,13,0.85) 100%)"></div>
    <div style="position:relative;z-index:1;padding:1rem">
      <div class="card-title">GPS Fix</div>
      <div class="card-value" id="gps-fix">—</div>
      <div class="detail">
        Lat: <span id="gps-lat">—</span> &nbsp;
        Lon: <span id="gps-lon">—</span>
      </div>
      <div class="detail">Sats: <span id="gps-sats">—</span></div>
    </div>
    <div id="minimap-crosshair" style="display:none;position:absolute;right:1.25rem;bottom:1.25rem;z-index:1;width:10px;height:10px;border:2px solid var(--primary);border-radius:50%;box-shadow:0 0 0 3px var(--primary-dim),0 0 8px var(--primary-glow)"></div>
  </div>
  </section>

  <section id="sect-upload">
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
    <div class="card-sm">
      <div class="card-title">Next Upload</div>
      <div class="card-value" style="font-size:1rem" id="next-upload">—</div>
    </div>
    <div class="card-sm">
      <div class="card-title">Lifetime Logged</div>
      <div class="card-value" id="total-logged">—</div>
    </div>
  </div>
  </section>

  <section id="sect-power">
  <p class="section-title">Power</p>
  <div class="card">
    <div class="stats-grid" style="margin-bottom:0">
      <div class="card-sm">
        <div class="card-title">Battery</div>
        <div class="card-value" id="batt-pct">—</div>
        <div class="detail"><span id="batt-volts">—</span>V</div>
        <div class="batt-bar"><div class="batt-fill" id="batt-fill" style="width:0%"></div></div>
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
  </section>

  <section id="sect-system">
  <p class="section-title">System</p>
  <div class="card">
    <div class="stats-grid" style="margin-bottom:0">
      <div class="card-sm">
        <div class="card-title">CPU Usage</div>
        <div class="card-value" id="cpu-pct">—</div>
        <div class="detail"><span id="cpu-mhz">—</span> MHz</div>
      </div>
      <div class="card-sm">
        <div class="card-title">Loop Time</div>
        <div class="card-value" style="font-size:1rem" id="loop-avg">—</div>
        <div class="detail">max <span id="loop-max">—</span></div>
      </div>
      <div class="card-sm">
        <div class="card-title">Free Heap</div>
        <div class="card-value" style="font-size:1rem" id="free-heap">—</div>
        <div class="detail">min <span id="min-heap">—</span></div>
      </div>
      <div class="card-sm">
        <div class="card-title">Disk</div>
        <div class="card-value" style="font-size:1rem" id="disk-free">—</div>
        <div class="detail">of <span id="disk-total">—</span></div>
      </div>
      <div class="card-sm">
        <div class="card-title">GPS Health</div>
        <div class="card-value" style="font-size:1rem" id="gps-health">—</div>
        <div class="detail"><span id="gps-cksum">—</span></div>
      </div>
    </div>
  </div>
  </section>

  <section id="sect-account">
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
      <div class="card-sm">
        <div class="card-title">Last Queried</div>
        <div class="card-value" style="font-size:1rem" id="rm-last-queried">—</div>
      </div>
    </div>
  </div>
  </section>

  <div class="version-info" id="version-info"></div>
</main>

<script>
(function(){
  var connLost = document.getElementById('conn-lost');
  var noToken = document.getElementById('no-token');
  function setText(id, val){ var el=document.getElementById(id); if(el) el.textContent=val; }
  function dotColor(id, ok){ var el=document.getElementById(id); if(!el)return; el.className='dot '+(ok?'dot-green':'dot-red'); }
  var mapLat=0,mapLon=0;

  function fmtUptime(s){
    if(s<60) return s+'s';
    if(s<3600) return Math.floor(s/60)+'m '+s%60+'s';
    var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);
    if(h<24) return h+'h '+m+'m';
    return Math.floor(h/24)+'d '+h%24+'h';
  }

  function fmtDuration(s){
    if(s<0) s=0;
    if(s<60) return s+'s ago';
    if(s<3600) return Math.floor(s/60)+'m '+Math.floor(s%60)+'s ago';
    var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);
    if(h<24) return h+'h '+m+'m ago';
    return Math.floor(h/24)+'d '+h%24+'h ago';
  }

  function fmtCountdown(s){
    if(s<=0) return 'Now';
    if(s<60) return 'in '+s+'s';
    if(s<3600) return 'in '+Math.floor(s/60)+'m '+Math.floor(s%60)+'s';
    var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);
    if(h<24) return 'in '+h+'h '+m+'m';
    return 'in '+Math.floor(h/24)+'d '+h%24+'h';
  }

  function refresh(){
    fetch('/api/status')
      .then(function(r){ if(!r.ok) throw new Error('bad'); return r.json(); })
      .then(function(d){
        connLost.style.display='none';
        noToken.style.display = d.upload_enabled ? 'none' : 'block';

        // Radiation
        setText('dose-rate', typeof d.dose_rate==='number' ? (d.dose_rate * 1000).toFixed(1) : '—');
        setText('count-rate', typeof d.count_rate==='number' ? d.count_rate.toFixed(2) : '—');

        // GPS
        var gpsOk = d.gps_fix===true||d.gps_fix===1;
        setText('gps-fix', gpsOk?'Fixed':'No Fix');
        var gpsEl=document.getElementById('gps-fix');
        if(gpsEl) gpsEl.className='card-value '+(gpsOk?'status-ok':'status-err');
        setText('gps-lat', gpsOk&&d.gps_lat!=null?d.gps_lat.toFixed(5):'—');
        setText('gps-lon', gpsOk&&d.gps_lon!=null?d.gps_lon.toFixed(5):'—');
        setText('gps-sats', d.gps_sats!=null?d.gps_sats:'—');
        dotColor('nav-gps', gpsOk);

        // Static map background
        var mbg=document.getElementById('minimap-bg');
        var mch=document.getElementById('minimap-crosshair');
        if(mbg&&gpsOk&&d.gps_lat!=null&&d.gps_lon!=null){
          var lt=d.gps_lat,ln=d.gps_lon;
          if(Math.abs(lt-mapLat)>0.0001||Math.abs(ln-mapLon)>0.0001){
            mapLat=lt;mapLon=ln;
            var z=14,n=Math.pow(2,z);
            var xt=Math.floor((ln+180)/360*n);
            var yt=Math.floor((1-Math.log(Math.tan(lt*Math.PI/180)+1/Math.cos(lt*Math.PI/180))/Math.PI)/2*n);
            mbg.style.backgroundImage='url(https://a.basemaps.cartocdn.com/dark_all/'+z+'/'+xt+'/'+yt+'.png)';
            mbg.style.opacity='0.35';
            if(mch) mch.style.display='block';
          }
        }

        // WiFi
        var wifiOk=d.wifi_connected===true||d.wifi_connected===1;
        setText('wifi-status', wifiOk?'Connected':'Disconnected');
        var wifiEl=document.getElementById('wifi-status');
        if(wifiEl) wifiEl.className='card-value '+(wifiOk?'status-ok':'status-err');
        setText('wifi-rssi', d.wifi_rssi!=null?d.wifi_rssi:'—');
        setText('wifi-ssid', d.wifi_ssid||'');
        dotColor('nav-wifi', wifiOk);

        // RadiaCode
        var rcOk=d.rc_connected===true||d.rc_connected===1;
        setText('rc-status', rcOk?'Connected':'Disconnected');
        var rcEl=document.getElementById('rc-status');
        if(rcEl) rcEl.className='card-value '+(rcOk?'status-ok':'status-err');
        setText('rc-source', rcOk?('via '+d.rc_source):'');
        dotColor('nav-usb', rcOk);

        // Buffer
        setText('buffer-pending', d.buffer_pending!=null?d.buffer_pending:'—');
        setText('buffer-total', d.buffer_total!=null?d.buffer_total:'—');
        if(d.last_upload_epoch){
          var agoS=Math.floor(Date.now()/1000)-d.last_upload_epoch;
          setText('last-upload', fmtDuration(agoS));
        } else {
          setText('last-upload', 'Never');
        }
        if(d.next_upload_epoch){
          var inS=d.next_upload_epoch-Math.floor(Date.now()/1000);
          setText('next-upload', fmtCountdown(inS));
        } else {
          setText('next-upload', '—');
        }
        setText('total-logged', d.total_readings_logged!=null?Number(d.total_readings_logged).toLocaleString():'—');

        // Battery
        var pct=d.battery_percent!=null?d.battery_percent:0;
        var volts=d.battery_voltage!=null?d.battery_voltage:0;
        setText('batt-pct', volts>2?pct+'%':'N/A');
        setText('batt-volts', volts>2?volts.toFixed(2):'—');
        var fill=document.getElementById('batt-fill');
        if(fill){
          fill.style.width=pct+'%';
          fill.style.background=pct>20?'linear-gradient(90deg,var(--primary),var(--secondary))':(pct>5?'var(--secondary)':'var(--danger)');
        }

        // Uptime + time
        setText('uptime', d.uptime!=null?fmtUptime(d.uptime):'—');
        var synced=d.time_synced===true||d.time_synced===1;
        var src=d.time_sync_source||'';
        setText('time-sync', synced?(src||'OK'):'Not synced');
        var tsEl=document.getElementById('time-sync');
        if(tsEl) tsEl.className='card-value '+(synced?'status-ok':'status-warn');

        // System perf
        setText('cpu-pct', d.cpu_usage_pct!=null?d.cpu_usage_pct+'%':'—');
        setText('cpu-mhz', d.cpu_mhz!=null?d.cpu_mhz:'—');
        setText('loop-avg', d.loop_avg_ms!=null?d.loop_avg_ms+' ms':'—');
        setText('loop-max', d.loop_max_ms!=null?d.loop_max_ms+' ms':'—');
        if(d.free_heap!=null){
          var fh=d.free_heap;
          setText('free-heap', fh>1024?(Math.round(fh/1024)+'K'):(fh+'B'));
        }
        if(d.min_free_heap!=null){
          var mh=d.min_free_heap;
          setText('min-heap', mh>1024?(Math.round(mh/1024)+'K'):(mh+'B'));
        }
        // Disk
        if(d.disk_total!=null&&d.disk_used!=null){
          var free=d.disk_total-d.disk_used;
          var tot=d.disk_total;
          function fmtB(b){return b>=1048576?(b/1048576).toFixed(1)+'MB':b>=1024?Math.round(b/1024)+'KB':b+'B';}
          setText('disk-free', fmtB(free)+' free');
          setText('disk-total', fmtB(tot));
        }
        // GPS health
        if(d.gps_sentences_fix!=null){
          setText('gps-health', d.gps_sentences_fix+' fixes');
        }
        if(d.gps_failed_cksum!=null){
          var fc=d.gps_failed_cksum;
          setText('gps-cksum', fc>0?(fc+' bad checksums'):'0 errors');
          var gcEl=document.getElementById('gps-cksum');
          if(gcEl) gcEl.style.color=fc>0?'var(--danger)':'';
        }

        // RadiaMaps
        setText('rm-username', d.rm_username||'—');
        setText('rm-subscription', d.rm_subscription||'—');
        setText('rm-lifetime', d.rm_lifetime_readings!=null?Number(d.rm_lifetime_readings).toLocaleString():'—');
        if(d.rm_last_queried){
          var dt=new Date(d.rm_last_queried*1000);
          var now=new Date();
          var diffS=Math.floor((now-dt)/1000);
          var ago;
          if(diffS<60) ago=diffS+'s ago';
          else if(diffS<3600) ago=Math.floor(diffS/60)+'m ago';
          else if(diffS<86400) ago=Math.floor(diffS/3600)+'h ago';
          else ago=Math.floor(diffS/86400)+'d ago';
          setText('rm-last-queried', ago);
        } else {
          setText('rm-last-queried', '—');
        }

        // Version
        var vi=document.getElementById('version-info');
        if(vi&&d.fw_version) vi.textContent='RadiaLog v'+d.fw_version;

        // Notify template engine
        if(window.RadiaLog&&window.RadiaLog.onData) window.RadiaLog.onData(d);
      })
      .catch(function(){
        connLost.style.display='block';
        dotColor('nav-wifi','');dotColor('nav-gps','');dotColor('nav-usb','');
      });
  }

  refresh();
  setInterval(refresh, 2000);

  // Status popup
  var popup=document.getElementById('status-popup'),ptimer=null,htimer=null;
  var ns=document.querySelector('.nav-status');
  ns.style.cursor='pointer';
  function fetchPopup(){
    popup.innerHTML='<div style="color:var(--text-tertiary)">Loading...</div>';
    popup.style.display='block';
    clearTimeout(ptimer);
    ptimer=setTimeout(function(){popup.style.display='none';},5000);
    fetch('/api/status').then(function(r){return r.json();}).then(function(d){
      var h='';
      h+='<div class="sr"><span class="sl">WiFi</span><span class="'+(d.wifi_connected?'st-ok':'st-err')+'" style="font-weight:600">'+(d.wifi_connected?(d.wifi_ssid||'Connected'):'Disconnected')+'</span></div>';
      if(d.wifi_sta_ip&&d.wifi_connected)h+='<div class="sr"><span class="sl">IP</span><span style="color:var(--cyan)">'+d.wifi_sta_ip+'</span></div>';
      h+='<div class="sr"><span class="sl">GPS</span><span class="'+(d.gps_fix?'st-ok':'st-warn')+'" style="font-weight:600">'+(d.gps_fix?d.gps_sats+' sats':'No Fix')+'</span></div>';
      h+='<div class="sr"><span class="sl">RadiaCode</span><span class="'+(d.rc_connected?'st-ok':'st-err')+'" style="font-weight:600">'+(d.rc_connected?(d.rc_source):'Disconnected')+'</span></div>';
      h+='<div class="sr"><span class="sl">Buffer</span><span style="color:var(--text-primary);font-weight:600">'+d.buffer_pending+' pending</span></div>';
      h+='<div class="sr"><span class="sl">Upload</span><span style="color:var(--text-secondary)">'+(d.last_upload||'Never')+'</span></div>';
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

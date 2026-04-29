#pragma once
#include <pgmspace.h>

// Self-Test page — guided post-assembly QA checks
const char SELFTEST_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RadiaLog &mdash; Self-Test</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0d1117;color:#c9d1d9;font-size:16px;line-height:1.5}
a{color:#58a6ff;text-decoration:none}a:hover{text-decoration:underline}
nav{background:#161b22;border-bottom:1px solid #30363d;padding:0 1rem;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.1rem;color:#f0f6fc;flex:1}
.nav-links{display:flex;gap:0.25rem;margin:0 0.75rem}
.nav-links a{color:#8b949e;padding:0.35rem 0.6rem;border-radius:6px;font-size:0.875rem;white-space:nowrap}
.nav-links a:hover,.nav-links a.active{color:#f0f6fc;background:#21262d;text-decoration:none}
.nav-status{display:flex;align-items:center;gap:0.5rem;flex-shrink:0}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;flex-shrink:0}
.dot-green{background:#3fb950}.dot-red{background:#f85149}.dot-gray{background:#484f58}.dot-amber{background:#d29922}
main{max-width:640px;margin:0 auto;padding:1rem}
.section-title{font-size:0.875rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin:1.5rem 0 0.5rem;border-bottom:1px solid #21262d;padding-bottom:0.25rem}
.section-title:first-child{margin-top:0.5rem}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:0.75rem 1rem;margin-bottom:0.5rem}
.btn{display:inline-block;padding:0.4rem 0.8rem;border-radius:6px;border:1px solid #30363d;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:0.8rem}
.btn:hover{background:#30363d}
.btn:disabled{opacity:0.5;cursor:not-allowed}
.btn-primary{background:#238636;border-color:#238636;color:#fff}
.btn-primary:hover{background:#2ea043}
.btn-danger{background:#da3633;border-color:#da3633;color:#fff}
.btn-danger:hover{background:#f85149}
.btn-sm{padding:0.25rem 0.5rem;font-size:0.75rem}
.toast{display:none;position:fixed;bottom:1rem;left:50%;transform:translateX(-50%);background:#238636;color:#fff;padding:0.5rem 1rem;border-radius:6px;font-size:0.875rem;z-index:200}
.toast.error{background:#da3633}
.test-row{display:flex;align-items:center;gap:0.75rem}
.test-info{flex:1;min-width:0}
.test-name{font-weight:600;font-size:0.95rem;color:#f0f6fc}
.test-desc{font-size:0.78rem;color:#8b949e;margin-top:0.1rem}
.test-detail{font-size:0.78rem;margin-top:0.25rem;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;word-break:break-word}
.test-detail.pass{color:#3fb950}.test-detail.fail{color:#f85149}.test-detail.run{color:#d29922}
.badge{font-size:0.7rem;padding:0.15rem 0.5rem;border-radius:10px;background:#21262d;color:#8b949e;text-transform:uppercase;letter-spacing:0.05em;flex-shrink:0}
.badge.pass{background:#1f6f37;color:#fff}
.badge.fail{background:#a4262c;color:#fff}
.badge.run{background:#9e6a03;color:#fff}
.badge.skip{background:#30363d;color:#8b949e}
.summary-bar{display:flex;gap:0.5rem;align-items:center;flex-wrap:wrap;margin-bottom:0.75rem}
.confirm-row{display:flex;gap:0.5rem;margin-top:0.5rem}
.dev-toggle{margin-left:auto;font-size:0.78rem;color:#8b949e;display:flex;align-items:center;gap:0.35rem;cursor:pointer}
.dev-toggle input{cursor:pointer}
.dev-pill{font-size:0.62rem;padding:0.05rem 0.35rem;border-radius:8px;background:#30363d;color:#d29922;text-transform:uppercase;letter-spacing:0.05em;margin-left:0.4rem;font-weight:700}
</style>
</head>
<body>
<nav>
  <span class="nav-brand">&#9762; RadiaLog</span>
  <div class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/debug">Debug</a>
    <a href="/settings">Settings</a>
    <a href="/data">Data</a>
    <a href="/self-test" class="active">Self-Test</a>
  </div>
  <div class="nav-status">
    <span class="dot dot-gray" id="nav-wifi" title="WiFi"></span>
    <span class="dot dot-gray" id="nav-gps" title="GPS"></span>
    <span class="dot dot-gray" id="nav-usb" title="USB"></span>
  </div>
</nav>
<main>

<p class="section-title">Self-Test</p>
<div class="card">
  <div class="summary-bar">
    <button class="btn btn-primary" onclick="runAll()" id="run-all-btn">Run All Tests</button>
    <button class="btn" onclick="resetAll()" id="reset-btn">Reset</button>
    <span id="summary" style="font-size:0.85rem;color:#8b949e"></span>
    <label class="dev-toggle" title="Show extra tests that require physical access to internal hardware (e.g. BOOT button).">
      <input type="checkbox" id="show-dev" onchange="applyDevToggle()"> Show developer tests
    </label>
  </div>
  <div style="font-size:0.78rem;color:#8b949e">Run after assembly to verify hardware. GPS test needs a clear sky view; BLE test needs a powered RadiaCode nearby.</div>
</div>

<div id="tests"></div>

<p class="section-title">After Testing</p>
<div class="card">
  <div style="font-size:0.78rem;color:#8b949e;margin-bottom:0.5rem">Wipe all device data before shipping.</div>
  <button class="btn btn-danger" onclick="if(confirm('Factory reset will erase ALL settings, WiFi credentials, paired devices, and buffered readings. This cannot be undone. Continue?'))factoryReset()">Factory Reset</button>
</div>

</main>
<div class="toast" id="toast"></div>

<script>
function toast(msg, err){
  var t=document.getElementById('toast');
  t.textContent=msg; t.className='toast'+(err?' error':''); t.style.display='block';
  setTimeout(function(){t.style.display='none';},2500);
}

// Test definitions. Each test exposes a run(ctx) that resolves to
// {status:'pass'|'fail'|'skip', detail:string}.
var TESTS = [
  {
    id:'battery', name:'Battery', desc:'Voltage between 3.0 V and 4.3 V',
    run:function(){
      return fetchStatus().then(function(s){
        var v=s.battery_voltage;
        if(v==null||isNaN(v))return {status:'fail',detail:'No reading'};
        var ok=v>=3.0&&v<=4.3;
        var chg=s.battery_charging?' (charging)':'';
        return {status:ok?'pass':'fail',detail:v.toFixed(2)+' V ('+s.battery_percent+'%)'+chg};
      });
    }
  },
  {
    id:'wifi_ap', name:'WiFi AP', desc:'Captive portal AP is active',
    run:function(){
      return fetchStatus().then(function(s){
        return {status:s.ap_active?'pass':'fail',detail:s.ap_active?'AP active':'AP not active'};
      });
    }
  },
  {
    id:'gps', name:'GPS Fix', desc:'Fix with at least 4 satellites (needs sky view)',
    run:function(setDetail){
      var deadline=Date.now()+30000;
      function poll(){
        return fetchStatus().then(function(s){
          var sats=s.gps_sats||0;
          var chars=s.gps_chars_processed||0;
          if(s.gps_fix&&sats>=4){
            return {status:'pass',detail:'Fix: '+sats+' sats, '+s.gps_lat.toFixed(5)+', '+s.gps_lon.toFixed(5)};
          }
          if(Date.now()>deadline){
            if(chars===0)return {status:'fail',detail:'No NMEA data — check GPS module / power'};
            return {status:'fail',detail:'No fix after 30s ('+sats+' sats, '+chars+' chars seen)'};
          }
          setDetail('waiting for fix... ('+sats+' sats, '+Math.round((deadline-Date.now())/1000)+'s left)','run');
          return new Promise(function(r){setTimeout(function(){r(poll());},1500);});
        });
      }
      return poll();
    }
  },
  {
    id:'ble_scan', name:'BLE Scan', desc:'Finds at least one RadiaCode (or one is already paired)',
    run:function(setDetail){
      return fetchStatus().then(function(s){
        // A paired RadiaCode stops advertising, so a scan can't see it.
        // If one is already connected via BLE, the radio is proven to work.
        if(s.ble_connected){
          return {status:'pass',detail:'RadiaCode already paired via BLE — scan skipped'};
        }
        setDetail('starting scan...','run');
        return fetch('/api/ble/scan',{method:'POST'}).then(function(r){return r.json();}).then(function(){
          var deadline=Date.now()+15000;
          function poll(){
            return fetch('/api/ble/scan').then(function(r){return r.json();}).then(function(d){
              if(d.status==='scanning'){
                if(Date.now()>deadline)return {status:'fail',detail:'Scan timed out'};
                setDetail('scanning...','run');
                return new Promise(function(r){setTimeout(function(){r(poll());},1000);});
              }
              var devs=(d.devices||[]).filter(function(x){return /^RC-/i.test(x.name||'');});
              if(devs.length===0){
                return {status:'fail',detail:'No RadiaCode (RC-*) found in scan'};
              }
              return {status:'pass',detail:devs.length+' RadiaCode found: '+devs.map(function(x){return x.name+' ('+x.rssi+'dBm)';}).join(', ')};
            });
          }
          return poll();
        }).catch(function(){return {status:'fail',detail:'Scan request failed'};});
      });
    }
  },
  {
    id:'radiacode', name:'RadiaCode Connected', desc:'A RadiaCode is paired via USB or BLE',
    run:function(){
      return fetchStatus().then(function(s){
        return {status:s.rc_connected?'pass':'fail',detail:s.rc_connected?'Source: '+s.rc_source:'No RadiaCode connected'};
      });
    }
  },
  {
    id:'live_reading', name:'Live Reading', desc:'count_rate > 0 within 10s',
    run:function(setDetail){
      var deadline=Date.now()+10000;
      function poll(){
        return fetchStatus().then(function(s){
          if(s.count_rate>0){
            return {status:'pass',detail:'count_rate='+s.count_rate.toFixed(2)+' cps, dose='+s.dose_rate.toFixed(3)+' uSv/h'};
          }
          if(Date.now()>deadline){
            return {status:'fail',detail:'No counts in 10s — is RadiaCode powered on?'};
          }
          setDetail('waiting for reading...','run');
          return new Promise(function(r){setTimeout(function(){r(poll());},1500);});
        });
      }
      return poll();
    }
  },
  {
    id:'button', name:'Boot Button', dev:true,
    desc:'Press the internal BOOT (GPIO0) button within 20s — needs case open',
    run:function(setDetail){
      return fetchStatus().then(function(s){
        var start=s.button_press_count||0;
        var deadline=Date.now()+20000;
        function poll(){
          return fetchStatus().then(function(s2){
            var now=s2.button_press_count||0;
            if(now>start)return {status:'pass',detail:'Press detected ('+now+' total)'};
            if(Date.now()>deadline)return {status:'fail',detail:'No button press detected'};
            setDetail('press the BOOT button now ('+Math.round((deadline-Date.now())/1000)+'s left)','run');
            return new Promise(function(r){setTimeout(function(){r(poll());},500);});
          });
        }
        return poll();
      });
    }
  },
  {
    id:'display', name:'Display', desc:'Wakes screen and asks visual confirmation',
    run:function(setDetail,ctx){
      return fetchStatus().then(function(s){
        if(!s.has_display){
          return {status:'skip',detail:'No display on this board'};
        }
        return fetch('/api/actions/display-test',{method:'POST'}).then(function(r){return r.json();}).then(function(){
          setDetail('look at the device — confirm below','run');
          return new Promise(function(resolve){
            ctx.askConfirm('Do you see the dashboard on the device screen?',function(yes){
              resolve(yes?{status:'pass',detail:'Confirmed visible'}:{status:'fail',detail:'Not visible'});
            });
          });
        });
      });
    }
  }
];

function fetchStatus(){
  return fetch('/api/status').then(function(r){return r.json();});
}

function renderTests(){
  var el=document.getElementById('tests');
  el.innerHTML='';
  TESTS.forEach(function(t){
    var card=document.createElement('div');
    card.className='card'+(t.dev?' dev-test':'');
    card.id='card-'+t.id;
    card.innerHTML=
      '<div class="test-row">'+
        '<div class="test-info">'+
          '<div class="test-name">'+t.name+(t.dev?'<span class="dev-pill">dev</span>':'')+'</div>'+
          '<div class="test-desc">'+t.desc+'</div>'+
          '<div class="test-detail" id="detail-'+t.id+'"></div>'+
          '<div class="confirm-row" id="confirm-'+t.id+'" style="display:none"></div>'+
        '</div>'+
        '<span class="badge" id="badge-'+t.id+'">pending</span>'+
        '<button class="btn btn-sm" onclick="runOne(\''+t.id+'\')" id="btn-'+t.id+'">Run</button>'+
      '</div>';
    el.appendChild(card);
  });
  applyDevToggle();
}

function applyDevToggle(){
  var show=document.getElementById('show-dev').checked;
  var nodes=document.querySelectorAll('.dev-test');
  for(var i=0;i<nodes.length;i++){nodes[i].style.display=show?'':'none';}
  updateSummary();
}

function visibleTests(){
  var showDev=document.getElementById('show-dev').checked;
  return TESTS.filter(function(t){return showDev||!t.dev;});
}

function setBadge(id,status){
  var b=document.getElementById('badge-'+id);
  b.className='badge '+status;
  b.textContent=status;
}

function setDetail(id,text,cls){
  var d=document.getElementById('detail-'+id);
  d.textContent=text||'';
  d.className='test-detail'+(cls?' '+cls:'');
}

function clearConfirm(id){
  var c=document.getElementById('confirm-'+id);
  c.innerHTML=''; c.style.display='none';
}

function askConfirm(id,question,cb){
  var c=document.getElementById('confirm-'+id);
  c.innerHTML='<span style="font-size:0.8rem;color:#c9d1d9;flex:1">'+question+'</span>'+
    '<button class="btn btn-sm btn-primary" id="conf-yes-'+id+'">Yes</button>'+
    '<button class="btn btn-sm btn-danger" id="conf-no-'+id+'">No</button>';
  c.style.display='flex';
  document.getElementById('conf-yes-'+id).onclick=function(){clearConfirm(id);cb(true);};
  document.getElementById('conf-no-'+id).onclick=function(){clearConfirm(id);cb(false);};
}

function runOne(id){
  var t=TESTS.find(function(x){return x.id===id;});
  if(!t)return;
  var btn=document.getElementById('btn-'+id);
  btn.disabled=true;
  setBadge(id,'run');
  setDetail(id,'running...','run');
  clearConfirm(id);
  var ctx={askConfirm:function(q,cb){askConfirm(id,q,cb);}};
  return Promise.resolve(t.run(function(text,cls){setDetail(id,text,cls);},ctx))
    .then(function(res){
      setBadge(id,res.status);
      setDetail(id,res.detail,res.status==='pass'?'pass':res.status==='fail'?'fail':'');
      btn.disabled=false;
      updateSummary();
      return res;
    })
    .catch(function(e){
      setBadge(id,'fail');
      setDetail(id,'error: '+e,'fail');
      btn.disabled=false;
      updateSummary();
      return {status:'fail'};
    });
}

function runAll(){
  var allBtn=document.getElementById('run-all-btn');
  allBtn.disabled=true; allBtn.textContent='Running...';
  resetAll();
  var queue=visibleTests();
  var i=0;
  function next(){
    if(i>=queue.length){
      allBtn.disabled=false; allBtn.textContent='Run All Tests';
      return;
    }
    runOne(queue[i].id).then(function(){i++;next();});
  }
  next();
}

function resetAll(){
  TESTS.forEach(function(t){
    setBadge(t.id,'pending');
    setDetail(t.id,'');
    clearConfirm(t.id);
  });
  updateSummary();
}

function updateSummary(){
  var counts={pass:0,fail:0,skip:0,pending:0,run:0};
  visibleTests().forEach(function(t){
    var b=document.getElementById('badge-'+t.id);
    if(!b)return;
    counts[b.textContent]=(counts[b.textContent]||0)+1;
  });
  var s=document.getElementById('summary');
  s.textContent=counts.pass+' passed, '+counts.fail+' failed, '+counts.skip+' skipped';
}

function factoryReset(){
  fetch('/api/actions/factory-reset',{method:'POST'})
    .then(function(r){return r.json();})
    .then(function(d){toast(d.message||'Reset complete',!d.success);})
    .catch(function(){toast('Factory reset failed',true);});
}

// Status dots in nav (mirrors other pages)
function refreshNavDots(){
  fetchStatus().then(function(s){
    var setDot=function(id,ok){var e=document.getElementById(id);if(e)e.className='dot '+(ok?'dot-green':'dot-red');};
    setDot('nav-wifi',s.wifi_connected||s.ap_active);
    setDot('nav-gps',s.gps_fix);
    setDot('nav-usb',s.rc_connected);
  }).catch(function(){});
}

renderTests();
updateSummary();
refreshNavDots();
setInterval(refreshNavDots,5000);
</script>
</body>
</html>)rawhtml";

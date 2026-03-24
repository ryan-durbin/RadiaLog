#pragma once
#include <pgmspace.h>

// Shared responsive CSS for all RadiaLog portal pages
const char SHARED_CSS[] PROGMEM = R"rawcss(
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0d1117;color:#c9d1d9;font-size:16px;line-height:1.5}
a{color:#58a6ff;text-decoration:none}
a:hover{text-decoration:underline}

/* Nav bar */
nav{background:#161b22;border-bottom:1px solid #30363d;padding:0 1rem;display:flex;align-items:center;height:52px;position:sticky;top:0;z-index:100}
.nav-brand{font-weight:700;font-size:1.1rem;color:#f0f6fc;flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nav-links{display:flex;gap:0.25rem;margin:0 0.75rem}
.nav-links a{color:#8b949e;padding:0.35rem 0.6rem;border-radius:6px;font-size:0.875rem;white-space:nowrap}
.nav-links a:hover,.nav-links a.active{color:#f0f6fc;background:#21262d;text-decoration:none}
.nav-status{display:flex;align-items:center;gap:0.5rem;flex-shrink:0}

/* Status dots */
.dot{width:10px;height:10px;border-radius:50%;display:inline-block;flex-shrink:0}
.dot-wifi,.dot-gps,.dot-usb{position:relative}
.dot-wifi::after,.dot-gps::after,.dot-usb::after{position:absolute;bottom:-18px;left:50%;transform:translateX(-50%);font-size:9px;color:#8b949e;white-space:nowrap}
.dot-wifi::after{content:'WiFi'}
.dot-gps::after{content:'GPS'}
.dot-usb::after{content:'USB'}
.dot-green,.status-ok{color:#3fb950}
.dot-green{background:#3fb950}
.dot-red,.status-err{color:#f85149}
.dot-red{background:#f85149}
.dot-gray{background:#484f58}
.dot-yellow{background:#d29922}

/* Main content */
main{max-width:720px;margin:0 auto;padding:1rem}

/* Cards */
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:1rem;margin-bottom:1rem}
.card-title{font-size:0.8rem;text-transform:uppercase;letter-spacing:0.05em;color:#8b949e;margin-bottom:0.5rem}
.card-value{font-size:1.6rem;font-weight:700;color:#f0f6fc}
.card-unit{font-size:0.875rem;color:#8b949e;margin-left:0.25rem}

/* Grid for stats */
.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:0.75rem;margin-bottom:1rem}

/* Forms */
label{display:block;margin-bottom:0.25rem;font-size:0.875rem;color:#8b949e}
input[type=text],input[type=password],input[type=number],input[type=url],select,textarea{
  width:100%;padding:0.5rem 0.75rem;background:#0d1117;border:1px solid #30363d;border-radius:6px;
  color:#c9d1d9;font-size:0.9rem;outline:none;transition:border-color .15s}
input:focus,select:focus,textarea:focus{border-color:#58a6ff}
.form-group{margin-bottom:0.875rem}

/* Buttons */
.btn{display:inline-flex;align-items:center;gap:0.35rem;padding:0.45rem 0.9rem;border-radius:6px;
  border:1px solid transparent;font-size:0.875rem;font-weight:500;cursor:pointer;transition:background .15s,opacity .15s}
.btn:disabled{opacity:0.5;cursor:not-allowed}
.btn-primary{background:#238636;border-color:#2ea043;color:#fff}
.btn-primary:hover:not(:disabled){background:#2ea043}
.btn-secondary{background:#21262d;border-color:#30363d;color:#c9d1d9}
.btn-secondary:hover:not(:disabled){background:#30363d}
.btn-danger{background:#b62324;border-color:#f85149;color:#fff}
.btn-danger:hover:not(:disabled){background:#da3633}
.btn-warning{background:#9e6a03;border-color:#d29922;color:#fff}
.btn-warning:hover:not(:disabled){background:#bb8009}
.btn-sm{padding:0.3rem 0.6rem;font-size:0.8rem}

/* Tables */
.table-wrap{overflow-x:auto;-webkit-overflow-scrolling:touch;border-radius:6px;border:1px solid #30363d}
table{width:100%;border-collapse:collapse;font-size:0.875rem}
thead{background:#21262d}
th{text-align:left;padding:0.6rem 0.75rem;font-weight:600;color:#8b949e;border-bottom:1px solid #30363d;white-space:nowrap}
td{padding:0.55rem 0.75rem;border-bottom:1px solid #21262d;vertical-align:middle}
tr:last-child td{border-bottom:none}
tbody tr:nth-child(even){background:rgba(255,255,255,.02)}
tbody tr:hover{background:#1c2128}

/* Log console */
.log-console{background:#0d1117;border:1px solid #30363d;border-radius:6px;height:380px;overflow-y:auto;
  font-family:'Courier New',Courier,monospace;font-size:0.8rem;padding:0.5rem}
.log-entry{padding:1px 0;white-space:pre-wrap;word-break:break-all;border-bottom:1px solid rgba(255,255,255,.03)}
.level-error{color:#f85149}
.level-warn{color:#d29922}
.level-info{color:#c9d1d9}
.level-debug{color:#484f58}

/* Filters bar */
.filters{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.75rem}
.filter-label{display:inline-flex;align-items:center;gap:0.3rem;font-size:0.8rem;cursor:pointer;
  background:#21262d;border:1px solid #30363d;border-radius:20px;padding:0.2rem 0.6rem;user-select:none}
.filter-label input{accent-color:#58a6ff}

/* Alerts / feedback */
.alert{padding:0.6rem 0.9rem;border-radius:6px;font-size:0.875rem;margin-bottom:0.75rem}
.alert-success{background:#1a3a28;border:1px solid #2ea043;color:#3fb950}
.alert-error{background:#3b1c1c;border:1px solid #f85149;color:#f85149}
.alert-info{background:#1a2e44;border:1px solid #58a6ff;color:#58a6ff}

/* Chart container */
.chart-wrap{position:relative;height:200px;margin-bottom:1rem}

/* Utility */
.text-muted{color:#8b949e}
.text-sm{font-size:0.8rem}
.mt-1{margin-top:.5rem}.mt-2{margin-top:1rem}.mb-1{margin-bottom:.5rem}.mb-2{margin-bottom:1rem}
.flex{display:flex}.gap-1{gap:.5rem}.gap-2{gap:1rem}.items-center{align-items:center}
.justify-between{justify-content:space-between}
.hidden{display:none!important}
.w-full{width:100%}
.divider{border:none;border-top:1px solid #30363d;margin:1rem 0}

/* Mobile tweaks */
@media(max-width:480px){
  .nav-links a{padding:0.3rem 0.45rem;font-size:0.8rem}
  .card-value{font-size:1.3rem}
  main{padding:.75rem}
}
)rawcss";

// Nav bar HTML template — replace %DEVICE_NAME% and %STATUS_INDICATORS% before sending
const char NAV_TEMPLATE[] PROGMEM = R"rawhtml(
<nav>
  <span class="nav-brand">&#x2622; %DEVICE_NAME%</span>
  <div class="nav-links">
    <a href="/">Home</a>
    <a href="/debug">Debug</a>
    <a href="/settings">Settings</a>
    <a href="/data">Data</a>
  </div>
  <div class="nav-status">%STATUS_INDICATORS%</div>
</nav>
)rawhtml";

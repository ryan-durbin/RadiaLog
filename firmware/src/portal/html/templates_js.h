#pragma once
#include <pgmspace.h>

// Template engine — served at /templates.js, loaded by all pages.
// Provides a theme picker in the nav bar and CSS overrides per template.
// Selection persisted in localStorage (no firmware/NVS involvement).
const char TEMPLATES_JS[] PROGMEM = R"rawjs(
(function(){
var T={
default:{name:'Default',css:''},
readings:{name:'Readings',css:
'main{display:flex;flex-direction:column}'+
'#sect-radiation{order:-1}'+
'#sect-radiation .stats-grid{grid-template-columns:1fr 1fr;gap:1rem}'+
'#sect-radiation .card-sm{padding:1.5rem;text-align:center}'+
'#sect-radiation .card-value{font-size:3rem}'+
'#sect-radiation .detail{font-size:1rem}'+
'#sect-location,#sect-connectivity,#sect-power,#sect-account{display:none}'+
'#sect-upload .section-title{margin-top:0.5rem}'
},
upload:{name:'Upload Monitor',css:
'main{display:flex;flex-direction:column}'+
'#sect-upload{order:-2}'+
'#sect-upload .stats-grid{grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:1rem}'+
'#sect-upload .card-sm{padding:1rem}'+
'#sect-upload .card-value{font-size:1.5rem;color:#58a6ff}'+
'#sect-upload .section-title{font-size:1rem;color:#58a6ff}'+
'#sect-radiation{order:-1}'+
'#sect-location,#sect-connectivity,#sect-account{display:none}'
},
compact:{name:'Compact',css:
'main{max-width:100%;padding:0.5rem}'+
'.card-sm{padding:0.4rem}'+
'.card-sm .card-value{font-size:1rem}'+
'.card-sm .card-title{font-size:0.65rem;margin-bottom:0.15rem}'+
'.stats-grid{gap:0.4rem;grid-template-columns:repeat(auto-fill,minmax(110px,1fr))}'+
'.section-title{margin:0.5rem 0 0.25rem;font-size:0.75rem}'+
'.card{padding:0.5rem;margin-bottom:0.5rem}'+
'.detail{font-size:0.7rem}'
},
pulse:{name:'Rad Pulse',css:
'body{overflow-x:hidden}'+
'body::before{content:"";position:fixed;top:-10%;left:-10%;right:-10%;bottom:-10%;'+
'z-index:-1;pointer-events:none;'+
'background:radial-gradient(circle at 50% 50%,var(--pulse-color,rgba(63,185,80,0.15)) 0%,transparent 70%);'+
'animation:radpulse var(--pulse-speed,3s) ease-in-out infinite}'+
'@keyframes radpulse{0%,100%{opacity:0.3;transform:scale(1)}50%{opacity:1;transform:scale(1.08)}}',
onData:function(d){
var c=d.count_rate||0;
var s=Math.max(0.3,3-c*0.1);
var r=document.documentElement;
r.style.setProperty('--pulse-speed',s+'s');
r.style.setProperty('--pulse-color',
c>10?'rgba(248,81,73,0.25)':c>3?'rgba(210,153,34,0.2)':'rgba(63,185,80,0.15)');
}}
};

var cur=localStorage.getItem('rl_tpl')||'default';
if(!T[cur])cur='default';
var sty=document.createElement('style');sty.id='tpl-css';document.head.appendChild(sty);

function apply(id){
cur=id;localStorage.setItem('rl_tpl',id);
sty.textContent=T[id]?T[id].css:'';
var s=document.getElementById('tpl-sel');if(s)s.value=id;
}

function init(){
var nl=document.querySelector('.nav-links');if(!nl)return;
var w=document.createElement('div');
w.style.cssText='display:flex;align-items:center;margin:0 0.5rem';
var s=document.createElement('select');s.id='tpl-sel';
s.title='Dashboard template';
s.style.cssText='padding:0.2rem 0.3rem;background:#0d1117;border:1px solid #30363d;'+
'border-radius:6px;color:#8b949e;font-size:0.75rem;outline:none;cursor:pointer';
for(var k in T){if(!T.hasOwnProperty(k))continue;
var o=document.createElement('option');o.value=k;o.textContent=T[k].name;
if(k===cur)o.selected=true;s.appendChild(o);}
s.onchange=function(){apply(this.value);};
w.appendChild(s);
nl.parentNode.insertBefore(w,nl.nextSibling);
}

apply(cur);
if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init);
else init();

window.RadiaLog=window.RadiaLog||{};
window.RadiaLog.onData=function(d){if(T[cur]&&T[cur].onData)T[cur].onData(d);};
})();
)rawjs";

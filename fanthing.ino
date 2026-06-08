#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <EEPROM.h>

#define tempSensor D7
#define sensorType DHT11

float currentTemp = 0;
float currentSpeed = 0;
unsigned long lastRead = 0;

float curveTemps[5]  = {20, 25, 30, 40, 50};
float curveSpeeds[5] = {0,  0,  40, 80, 100};
int curveCount = 5;

const char PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Control Page</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600&display=swap');
  :root{--bg:#0a0e14;--panel:#111820;--border:#1e3a4a;--accent:#00d4ff;--accent2:#ff6b35;--text:#c8d8e0;--muted:#4a6070;--danger:#ff3b5c;}
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;font-weight:300;min-height:100vh;padding:24px 16px;}
  h1{font-family:'Share Tech Mono',monospace;font-size:1.4rem;color:var(--accent);letter-spacing:.15em;text-transform:uppercase;margin-bottom:6px;}
  .subtitle{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--muted);letter-spacing:.2em;margin-bottom:28px;}
  .stats{display:flex;gap:16px;margin-bottom:24px;}
  .stat{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:14px 20px;flex:1;}
  .stat-label{font-family:'Share Tech Mono',monospace;font-size:.65rem;color:var(--muted);letter-spacing:.2em;text-transform:uppercase;margin-bottom:6px;}
  .stat-value{font-family:'Share Tech Mono',monospace;font-size:2rem;color:var(--accent);line-height:1;}
  .stat-value.fan{color:var(--accent2);}
  .stat-unit{font-size:.8rem;color:var(--muted);margin-left:4px;}
  .graph-container{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:16px;margin-bottom:20px;}
  canvas{width:100%;height:200px;display:block;}
  .points-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;}
  .section-label{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--muted);letter-spacing:.2em;text-transform:uppercase;}
  .btn-add{background:transparent;border:1px solid var(--accent);color:var(--accent);font-family:'Share Tech Mono',monospace;font-size:.7rem;letter-spacing:.1em;padding:5px 12px;border-radius:4px;cursor:pointer;transition:all .2s;}
  .btn-add:hover{background:var(--accent);color:var(--bg);}
  .btn-add:disabled{opacity:.3;cursor:not-allowed;}
  .points-list{display:flex;flex-direction:column;gap:8px;margin-bottom:20px;}
  .point-row{display:flex;align-items:center;gap:10px;background:var(--panel);border:1px solid var(--border);border-radius:6px;padding:10px 14px;}
  .point-row label{font-family:'Share Tech Mono',monospace;font-size:.65rem;color:var(--muted);width:20px;text-align:center;}
  .point-row input{background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-family:'Share Tech Mono',monospace;font-size:.9rem;padding:6px 10px;width:70px;text-align:center;transition:border-color .2s;}
  .point-row input:focus{outline:none;border-color:var(--accent);}
  .point-sep{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--muted);}
  .point-unit{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--muted);width:24px;}
  .btn-remove{background:transparent;border:1px solid var(--danger);color:var(--danger);font-family:'Share Tech Mono',monospace;font-size:.7rem;padding:4px 8px;border-radius:4px;cursor:pointer;margin-left:auto;transition:all .2s;}
  .btn-remove:hover{background:var(--danger);color:white;}
  .btn-save{width:100%;background:var(--accent);border:none;color:var(--bg);font-family:'Share Tech Mono',monospace;font-size:.85rem;letter-spacing:.2em;text-transform:uppercase;padding:14px;border-radius:6px;cursor:pointer;transition:all .2s;font-weight:bold;}
  .btn-save:hover{background:#33ddff;}
  .toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%) translateY(80px);background:var(--accent);color:var(--bg);font-family:'Share Tech Mono',monospace;font-size:.75rem;letter-spacing:.15em;padding:10px 24px;border-radius:20px;transition:transform .3s ease;pointer-events:none;}
  .toast.show{transform:translateX(-50%) translateY(0);}
</style>
</head>
<body>
<h1>Fan Control</h1>
<div class="subtitle">I am not a Web Developer. This page was made by AI.</div>
<div class="stats">
  <div class="stat"><div class="stat-label">Temperature</div><div class="stat-value" id="tempVal">--<span class="stat-unit">°C</span></div></div>
  <div class="stat"><div class="stat-label">Fan Speed</div><div class="stat-value fan" id="fanVal">--<span class="stat-unit">%</span></div></div>
</div>
<div class="graph-container">
  <div class="section-label" style="margin-bottom:10px">Preview</div>
  <canvas id="graph"></canvas>
</div>
<div class="points-header">
  <div class="section-label">Temperature adjustment points</div>
  <button class="btn-add" id="addBtn" onclick="addPoint()">+ Add adjustment point</button>
</div>
<div class="points-list" id="pointsList"></div>
<button class="btn-save" onclick="saveCurve()">Save Fan Curve</button>
<div class="toast" id="toast">Saved!</div>
<script>
let points=[{t:20,s:0},{t:25,s:0},{t:30,s:40},{t:40,s:80},{t:50,s:100}];
let currentTemp=null,currentSpeed=null;
function renderPoints(){
  const list=document.getElementById('pointsList');
  list.innerHTML='';
  points.forEach((p,i)=>{
    const row=document.createElement('div');
    row.className='point-row';
    row.innerHTML=`<label>#${i+1}</label><input type="number" value="${p.t}" min="0" max="100" oninput="updatePoint(${i},'t',this.value)"><span class="point-unit">°C</span><span class="point-sep">→</span><input type="number" value="${p.s}" min="0" max="100" oninput="updatePoint(${i},'s',this.value)"><span class="point-unit">%</span>${points.length>2?`<button class="btn-remove" onclick="removePoint(${i})">✕</button>`:''}`;
    list.appendChild(row);
  });
  document.getElementById('addBtn').disabled=points.length>=5;
  drawGraph();
}
function updatePoint(i,key,val){points[i][key]=parseFloat(val)||0;drawGraph();}
function addPoint(){if(points.length>=5)return;points.push({t:points[points.length-1].t+5,s:100});renderPoints();}
function removePoint(i){if(points.length<=2)return;points.splice(i,1);renderPoints();}
function interpolate(t){
  if(t<=points[0].t)return points[0].s;
  if(t>=points[points.length-1].t)return points[points.length-1].s;
  for(let i=0;i<points.length-1;i++){
    if(t>=points[i].t&&t<=points[i+1].t){
      const r=(t-points[i].t)/(points[i+1].t-points[i].t);
      return points[i].s+r*(points[i+1].s-points[i].s);
    }
  }
  return 0;
}
function drawGraph(){
  const canvas=document.getElementById('graph');
  const dpr=window.devicePixelRatio||1;
  const W=canvas.offsetWidth,H=canvas.offsetHeight||200;
  canvas.width=W*dpr;canvas.height=H*dpr;
  const ctx=canvas.getContext('2d');
  ctx.scale(dpr,dpr);
  const pad={top:10,right:16,bottom:30,left:36};
  const gw=W-pad.left-pad.right,gh=H-pad.top-pad.bottom;
  ctx.strokeStyle='#1e3a4a';ctx.lineWidth=1;
  for(let i=0;i<=4;i++){
    const y=pad.top+(gh/4)*i;
    ctx.beginPath();ctx.moveTo(pad.left,y);ctx.lineTo(pad.left+gw,y);ctx.stroke();
    ctx.fillStyle='#4a6070';ctx.font='9px Share Tech Mono,monospace';ctx.textAlign='right';
    ctx.fillText((100-i*25)+'%',pad.left-4,y+4);
  }
  const minT=Math.min(...points.map(p=>p.t))-5;
  const maxT=Math.max(...points.map(p=>p.t))+5;
  function tx(t){return pad.left+((t-minT)/(maxT-minT))*gw;}
  function ty(s){return pad.top+gh-(s/100)*gh;}
  ctx.fillStyle='#4a6070';ctx.font='9px Share Tech Mono,monospace';ctx.textAlign='center';
  for(let i=0;i<=4;i++){const t=minT+(maxT-minT)*i/4;ctx.fillText(Math.round(t)+'°',tx(t),H-pad.bottom+16);}
  const grad=ctx.createLinearGradient(0,pad.top,0,pad.top+gh);
  grad.addColorStop(0,'rgba(0,212,255,0.3)');grad.addColorStop(1,'rgba(0,212,255,0)');
  ctx.beginPath();ctx.moveTo(tx(points[0].t),ty(points[0].s));
  for(let i=1;i<points.length;i++){const cpx=(tx(points[i-1].t)+tx(points[i].t))/2;ctx.bezierCurveTo(cpx,ty(points[i-1].s),cpx,ty(points[i].s),tx(points[i].t),ty(points[i].s));}
  ctx.lineTo(tx(points[points.length-1].t),pad.top+gh);ctx.lineTo(tx(points[0].t),pad.top+gh);
  ctx.closePath();ctx.fillStyle=grad;ctx.fill();
  ctx.beginPath();ctx.moveTo(tx(points[0].t),ty(points[0].s));
  for(let i=1;i<points.length;i++){const cpx=(tx(points[i-1].t)+tx(points[i].t))/2;ctx.bezierCurveTo(cpx,ty(points[i-1].s),cpx,ty(points[i].s),tx(points[i].t),ty(points[i].s));}
  ctx.strokeStyle='#00d4ff';ctx.lineWidth=2;ctx.stroke();
  points.forEach(p=>{ctx.beginPath();ctx.arc(tx(p.t),ty(p.s),4,0,Math.PI*2);ctx.fillStyle='#00d4ff';ctx.fill();ctx.strokeStyle='#0a0e14';ctx.lineWidth=2;ctx.stroke();});
  if(currentTemp!==null&&currentTemp>=minT&&currentTemp<=maxT){
    const x=tx(currentTemp);
    ctx.beginPath();ctx.moveTo(x,pad.top);ctx.lineTo(x,pad.top+gh);
    ctx.strokeStyle='#ff6b35';ctx.lineWidth=1.5;ctx.setLineDash([4,3]);ctx.stroke();ctx.setLineDash([]);
    ctx.beginPath();ctx.arc(x,ty(interpolate(currentTemp)),5,0,Math.PI*2);ctx.fillStyle='#ff6b35';ctx.fill();
  }
}
function saveCurve(){
  const sorted=[...points].sort((a,b)=>a.t-b.t);
  fetch('/save?temps='+sorted.map(p=>p.t).join(',')+'&speeds='+sorted.map(p=>p.s).join(','))
    .then(r=>r.text()).then(()=>{const t=document.getElementById('toast');t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2000);});
}
function pollData(){
  fetch('/data').then(r=>r.json()).then(d=>{
    currentTemp=d.temp;currentSpeed=d.speed;
    document.getElementById('tempVal').innerHTML=d.temp.toFixed(1)+'<span class="stat-unit">°C</span>';
    document.getElementById('fanVal').innerHTML=Math.round(d.speed)+'<span class="stat-unit">%</span>';
    drawGraph();
  }).catch(()=>{});
}
renderPoints();pollData();setInterval(pollData,3000);
</script>
</body>
</html>
)rawhtml";

float interpolate(float t) {
  if (t <= curveTemps[0]) return curveSpeeds[0];
  if (t >= curveTemps[curveCount-1]) return curveSpeeds[curveCount-1];
  for (int i = 0; i < curveCount-1; i++) {
    if (t >= curveTemps[i] && t <= curveTemps[i+1]) {
      float ratio = (t - curveTemps[i]) / (curveTemps[i+1] - curveTemps[i]);
      return curveSpeeds[i] + ratio * (curveSpeeds[i+1] - curveSpeeds[i]);
    }
  }
  return 0;
}

void setFanSpeed(float percent) {
  static float lastSpeed = 0;

    if (percent < 10.0) {
      analogWrite(D1, 0);
    } else {
    if(lastSpeed < 10.0){
      analogWrite(D1, 1023);
      delay(200);
    }
    analogWrite(D1, (int)(percent / 100.0 * 1023.0));
  }
}

void saveToFlash() {
  EEPROM.put(0, curveTemps);
  EEPROM.put(50, curveSpeeds);
  EEPROM.commit();
}

void loadFromFlash() {
  EEPROM.begin(512);
  EEPROM.get(0, curveTemps);
  EEPROM.get(50, curveSpeeds);
}


ESP8266WebServer server(80);
DHT dht(tempSensor, sensorType);

void setup() {
  Serial.begin(115200);
  loadFromFlash();
  Serial.println("ready!");

  //temp sensor
  dht.begin();
  //motor
  analogWriteFreq(25000);
  pinMode(D1, OUTPUT);
  //wifi and webpage
  WiFi.begin("s", "p");
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print("wait..");
  }
  Serial.println("Connected to the network.");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send_P(200, "text/html", PAGE);
  });

  server.on("/data", []() {
    String json = "{\"temp\":" + String(currentTemp, 1) + ",\"speed\":" + String(currentSpeed, 1) + "}";
    server.send(200, "application/json", json);
  });

  server.on("/save", []() {
    String tempsStr  = server.arg("temps");
    String speedsStr = server.arg("speeds");
    int count = 0;
    int tIdx = 0, sIdx = 0;
    while (tIdx < (int)tempsStr.length() && count < 5) {
      int comma = tempsStr.indexOf(',', tIdx);
      if (comma == -1) comma = tempsStr.length();
      curveTemps[count] = tempsStr.substring(tIdx, comma).toFloat();
      tIdx = comma + 1;
      int sComma = speedsStr.indexOf(',', sIdx);
      if (sComma == -1) sComma = speedsStr.length();
      curveSpeeds[count] = speedsStr.substring(sIdx, sComma).toFloat();
      sIdx = sComma + 1;
      count++;
    }
    if (count >= 2) curveCount = count;
    saveToFlash();
    server.send(200, "text/plain", "ok");
  });

  server.begin();
  Serial.println("well, init was indeed a success.");
}

void loop() {

  server.handleClient();
  //print and update values
  if (millis() - lastRead > 3000) {
    lastRead = millis();
    float t = dht.readTemperature();
    if (!isnan(t)) {
      currentTemp = t;
      currentSpeed = interpolate(t);
      setFanSpeed(currentSpeed);
      Serial.print("Temp: "); Serial.print(t);
      Serial.print("C  Fan: "); Serial.print(currentSpeed); Serial.println("%");
    } else {
      Serial.println("haha fail");
    }
  }
}



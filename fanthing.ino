  // How to use from source:
  // Install required libraries
  // Check if you already use the predefined pins or change them to your own
  // Also, change to the correct timezone of yours in the NTPClient function at the top, for example: NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800); (setting 10800 is GMT+3.)
  // Flash the code to your ESP8266-12E
  // On first boot, the ESP will create a hotspot named "FanControl". Connect to it from your phone (do not use pc, it redirects to MSN for some reason.), enter your wifi credentials. 
  // It'll save and the wifi network will dissappear, then the dashboard is available on http://fancontrol.local from any device on the same network connection.
  // If the website doesn't load, check your wifi connections again. If the ESP failed to connect, then it'll reboot and remake the setup hotspot.
  // Upon entering that website in your browser, you'll get to a dashboard showing the current info (Temp, if fan is on, the current temp limit, etc.)
  // If the website doesn't load, graphs or temperature readings are missing, check your serial monitor if you get an error saying "Failed to get a reading from the DHT11, please ensure that your sensor is connected check and if wiring is correct!".
  // From there, you can change anything you'd like (if you are changing the temperature limit, remember to click save) and download the past recorded temperatures in the form of a csv file. (click 24h -> download csv)
  // For the 24h to graph show up, it'll take around 10 minutes or so, and a reading will be taken every 5 minutes.
  // Enjoy!

  #include <WiFiManager.h>
  #include <ESP8266WebServer.h>
  #include "DHT.h"
  #include <EEPROM.h>
  #include <NTPClient.h>
  #include <WiFiUDP.h>
  #include <ESP8266mDNS.h>

  // Define the pins for your temp sensor and motor, if you don't use these already.
  #define tempSensor D2
  #define fanPin D1
  #define sensorType DHT11
  #define tempHistorySize 400
  #define dailyHistorySize 288

  float tempHistory[tempHistorySize];
  float dailyTemps[dailyHistorySize];
  float currentTemp = 0;
  float onTemp = 30.0;
  float offTemp = 28.0;


  unsigned long lastRead = 0;
  unsigned long dailyTimes[dailyHistorySize];
  unsigned long lastDailyRead = 0;

  bool fanOn = false;
  bool midnightResetDone = false;

  int historyCount = 0;
  int historyIndex = 0;
  int dailyCount = 0;
  int dailyIndex = 0;
  int utcOffset = 10800;

  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffset);

  //Dashboard page
  const char PAGE[] PROGMEM = R"rawhtml(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control Dashboard</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Raleway:wght@300;400;600&display=swap');
    :root{--bg:#f0fafa;--panel:#ffffff;--border:#b2ede0;--accent:#00bfae;--accent2:#26d9b8;--text:#1a3a35;--muted:#7ab8ad;--danger:#e05555;--on:#00c49a;--off:#aaaaaa;}
    *{box-sizing:border-box;margin:0;padding:0;}
    body{background:var(--bg);color:var(--text);font-family:'Raleway',sans-serif;font-weight:300;min-height:100vh;padding:24px 16px;}
    h1{font-size:1.4rem;color:var(--accent);letter-spacing:.1em;margin-bottom:4px;font-weight:600;}
    .subtitle{font-size:.7rem;color:var(--muted);letter-spacing:.2em;margin-bottom:28px;}
    .stats{display:flex;gap:16px;margin-bottom:24px;}
    .stat{background:var(--panel);border:1px solid var(--border);border-radius:10px;padding:14px 20px;flex:1;box-shadow:0 2px 8px rgba(0,191,174,0.07);}
    .stat-label{font-size:.65rem;color:var(--muted);letter-spacing:.2em;margin-bottom:6px;}
    .stat-value{font-size:2rem;color:var(--accent);line-height:1;font-weight:600;}
    .stat-value.fan-status{font-size:1.4rem;}
    .stat-value.fan-on{color:var(--on);}
    .stat-value.fan-off{color:var(--off);}
    .stat-unit{font-size:.8rem;color:var(--muted);margin-left:4px;font-weight:300;}
    .graph-container{background:var(--panel);border:1px solid var(--border);border-radius:10px;padding:16px;margin-bottom:20px;box-shadow:0 2px 8px rgba(0,191,174,0.07);}
    .section-label{font-size:.7rem;color:var(--muted);letter-spacing:.2em;margin-bottom:10px;}
    canvas{width:100%;height:200px;display:block;}
    .threshold-container{background:var(--panel);border:1px solid var(--border);border-radius:10px;padding:16px;margin-bottom:20px;box-shadow:0 2px 8px rgba(0,191,174,0.07);}
    .threshold-row{display:flex;align-items:center;gap:12px;margin-top:10px;}
    .threshold-row input{background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--text);font-family:'Raleway',sans-serif;font-size:1.1rem;font-weight:600;padding:8px 14px;width:90px;text-align:center;transition:border-color .2s;}
    .threshold-row input:focus{outline:none;border-color:var(--accent);}
    .threshold-unit{font-size:.9rem;color:var(--muted);}
    .threshold-hint{font-size:.7rem;color:var(--muted);margin-top:8px;}
    .btn-save{width:100%;background:var(--accent);border:none;color:white;font-family:'Raleway',sans-serif;font-size:.9rem;letter-spacing:.15em;padding:14px;border-radius:8px;cursor:pointer;transition:all .2s;font-weight:600;margin-top:4px;}
    .btn-save:hover{background:var(--accent2);}
    .toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%) translateY(80px);background:var(--accent);color:white;font-size:.75rem;letter-spacing:.15em;padding:10px 24px;border-radius:20px;transition:transform .3s ease;pointer-events:none;font-family:'Raleway',sans-serif;}
    .toast.show{transform:translateX(-50%) translateY(0);}
    .graph-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;}
    .graph-tabs{display:flex;gap:6px;}
    .tab{background:transparent;border:1px solid var(--border);color:var(--muted);font-family:'Raleway',sans-serif;font-size:.7rem;padding:4px 12px;border-radius:4px;cursor:pointer;transition:all .2s;}
    .tab.active{background:var(--accent);border-color:var(--accent);color:white;}
    .btn-export{width:100%;background:transparent;border:1px solid var(--accent);color:var(--accent);font-family:'Raleway',sans-serif;font-size:.8rem;padding:8px;border-radius:6px;cursor:pointer;margin-top:10px;transition:all .2s;}
    .btn-export:hover{background:var(--accent);color:white;}
  </style>
  </head>
  <body>
  <h1>Fan Control</h1>
  <div class="subtitle">Running on an ESP8266.<br>Please note: this page was made by AI.</div>
  <div class="stats">
    <div class="stat"><div class="stat-label">Temperature</div><div class="stat-value" id="tempVal">--<span class="stat-unit">°C</span></div></div>
    <div class="stat"><div class="stat-label">Fan</div><div class="stat-value fan-status fan-off" id="fanVal">OFF</div></div>
  </div>
  <div class="graph-container">
    <div class="graph-header">
      <div class="section-label">Temperature History</div>
      <div class="graph-tabs">
        <button class="tab active" onclick="setTab('realtime')">Realtime</button>
        <button class="tab" onclick="setTab('daily')">24h</button>
    </div>
  </div>
  <canvas id="graph"></canvas>
  <button class="btn-export" id="exportBtn" style="display:none" onclick="window.location='/export'">Download CSV</button>
</div>
  <div class="threshold-container">
    <div class="section-label">Temperature threshold</div>
    <div class="threshold-row">
      <input type="number" id="onTempInput" value="30" min="0" max="100" oninput="userEditing=true">
      <span class="threshold-unit">°C</span>
    </div>
    <div class="threshold-hint">Fan turns off 2°C below this value</div>
  </div>
  
  <div class="threshold-container">
    <div class="section-label">Timezone (UTC offset in hours)</div>
      <div class="threshold-row">
        <input type="number" id="tzInput" value="3" min="-12" max="14" oninput="tzEditing=true">
        <span class="threshold-unit">hrs</span>
      </div>
      <div class="threshold-hint">e.g. GMT+3 = 3, GMT-5 = -5</div>
    </div>
  <button class="btn-save" onclick="saveAll()">Save All Settings</button>
  <div class="toast" id="toast">Saved!</div>
  <script>
    let tempHistory=[];
    let dailyHistory=[];
    let onTemp=30;
    let userEditing=false;
    let currentTab='realtime';
    let tzEditing=false;



    function setTab(tab){
      currentTab=tab;
      document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById('exportBtn').style.display=tab==='daily'?'block':'none';
      drawGraph();
    }
    function drawGraph(){
      const canvas=document.getElementById('graph');
      const dpr=window.devicePixelRatio||1;
      const W=canvas.offsetWidth,H=canvas.offsetHeight||200;
      canvas.width=W*dpr;canvas.height=H*dpr;
      const ctx=canvas.getContext('2d');
      ctx.scale(dpr,dpr);
      const pad={top:10,right:16,bottom:32,left:46};
      const gw=W-pad.left-pad.right,gh=H-pad.top-pad.bottom;
      const data = currentTab==='realtime' ? tempHistory : dailyHistory.map(d=>d.temp);
      const labels = currentTab==='daily' ? dailyHistory.map(d=>d.time) : null;
      if(data.length<2){
        ctx.fillStyle='#b2ede0';ctx.font='13px Raleway,sans-serif';ctx.textAlign='center';
        ctx.fillText('Waiting for data...',W/2,H/2);return;
      }
      const minT=Math.min(...data)-2;
      const maxT=Math.max(...data,onTemp)+2;
      function tx(i){return pad.left+(i/(data.length-1))*gw;}
      function ty(t){return pad.top+gh-((t-minT)/(maxT-minT))*gh;}
      ctx.strokeStyle='#e0f5f0';ctx.lineWidth=1;
      for(let i=0;i<=4;i++){
        const y=pad.top+(gh/4)*i;
        ctx.beginPath();ctx.moveTo(pad.left,y);ctx.lineTo(pad.left+gw,y);ctx.stroke();
        const val=maxT-(maxT-minT)*i/4;
        ctx.fillStyle='#7ab8ad';ctx.font='11px Raleway,sans-serif';ctx.textAlign='right';
        ctx.fillText(val.toFixed(1)+'°',pad.left-4,y+4);
      }
      ctx.fillStyle='#7ab8ad';ctx.font='11px Raleway,sans-serif';ctx.textAlign='center';
      if(labels){
        const step=Math.ceil(labels.length/5);
        for(let i=0;i<labels.length;i+=step){
          ctx.fillText(labels[i],tx(i),H-pad.bottom+16);
        }
        ctx.fillText(labels[labels.length-1],tx(labels.length-1),H-pad.bottom+16);
      } else {
        ctx.fillText('oldest',pad.left,H-pad.bottom+16);
        ctx.fillText('newest',pad.left+gw,H-pad.bottom+16);
      }

      if(onTemp>=minT&&onTemp<=maxT){
        const y=ty(onTemp);
        ctx.beginPath();ctx.moveTo(pad.left,y);ctx.lineTo(pad.left+gw,y);
        ctx.strokeStyle='#00c49a';ctx.lineWidth=1.5;ctx.setLineDash([5,4]);ctx.stroke();ctx.setLineDash([]);
        ctx.fillStyle='#00c49a';ctx.font='11px Raleway,sans-serif';ctx.textAlign='left';
        ctx.fillText('ON at '+onTemp+'°',pad.left+4,y-4);
      }
      const grad=ctx.createLinearGradient(0,pad.top,0,pad.top+gh);
      grad.addColorStop(0,'rgba(0,191,174,0.25)');grad.addColorStop(1,'rgba(0,191,174,0)');
      ctx.beginPath();ctx.moveTo(tx(0),ty(data[0]));
      for(let i=1;i<data.length;i++){const cpx=(tx(i-1)+tx(i))/2;ctx.bezierCurveTo(cpx,ty(data[i-1]),cpx,ty(data[i]),tx(i),ty(data[i]));}
      ctx.lineTo(tx(data.length-1),pad.top+gh);ctx.lineTo(tx(0),pad.top+gh);
      ctx.closePath();ctx.fillStyle=grad;ctx.fill();
      ctx.beginPath();ctx.moveTo(tx(0),ty(data[0]));
      for(let i=1;i<data.length;i++){const cpx=(tx(i-1)+tx(i))/2;ctx.bezierCurveTo(cpx,ty(data[i-1]),cpx,ty(data[i]),tx(i),ty(data[i]));}
      ctx.strokeStyle='#00bfae';ctx.lineWidth=2;ctx.stroke();
    }

    function saveAll() {
  const temp = document.getElementById('onTempInput').value;
  const tz = document.getElementById('tzInput').value;
  

  fetch(`/saveAll?ontemp=${temp}&offset=${tz*3600}`)
    .then(r => r.text())
    .then(() => {
      const t = document.getElementById('toast');
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 2000);
      drawGraph();
    });
}

    function pollData(){
      fetch('/data').then(r=>r.json()).then(d=>{
        tempHistory=d.history;
        onTemp=d.onTemp;
        if(!userEditing) document.getElementById('onTempInput').value=onTemp;
        document.getElementById('tempVal').innerHTML=d.temp.toFixed(1)+'<span class="stat-unit">°C</span>';
        const fanEl=document.getElementById('fanVal');
        fanEl.textContent=d.fanOn?'ON':'OFF';
        fanEl.className='stat-value fan-status '+(d.fanOn?'fan-on':'fan-off');
        if(currentTab==='realtime') drawGraph();
        if(!tzEditing) document.getElementById('tzInput').value=d.utcOffset;
      }).catch(()=>{});
      if(currentTab==='daily'){
        fetch('/daily').then(r=>r.json()).then(d=>{
          dailyHistory=d.data;
          drawGraph();
        }).catch(()=>{});
      }
    }

    pollData();setInterval(pollData,3000);
  </script>
  </body>
  </html>
  )rawhtml";

  void updFanStat(float t) {
    if(!fanOn && t >= onTemp) {
      fanOn = true;
      digitalWrite(fanPin, 1);
    }
    else if(fanOn && t <= offTemp){
      fanOn = false;
      digitalWrite(fanPin, 0);
    }
  }

  //These functions save info to EEPROM/Flash, please ensure that if you modify this code not to spam EEPROM R/W, otherwise your ESP's EEPROM will fail quickly.

  void saveToFlash() {
    EEPROM.begin(512);
    EEPROM.put(0, onTemp);
    EEPROM.put(10, utcOffset);
    EEPROM.commit();
    EEPROM.end();
  }

  void loadFromFlash() {
    EEPROM.begin(512);
    float saved;
    EEPROM.get(0, saved);
    int savedOffset;
    EEPROM.get(10, savedOffset);
    if(savedOffset >= -43200 && savedOffset <= 50400){
      utcOffset = savedOffset;
    }
    Serial.println(saved);
    EEPROM.end();
    if(!isnan(saved) && saved > 0 && saved < 100){
      onTemp = saved;
      offTemp = saved -2.0;
    }
  }

  //This is a function to wipe the 24h graph at midnight, the old CSV file will also be wiped.

  void MidnightReset() {
    time_t now = timeClient.getEpochTime();
    struct tm* t = localtime(&now);
    if (t->tm_hour == 0 && t->tm_min == 0 && t->tm_sec < 10){
      dailyCount = 0;
      dailyIndex = 0;
      midnightResetDone = true;
      Serial.println("24h history wiped");
    }
    else{
      midnightResetDone = false;
    }
  }

    ESP8266WebServer server(80);
    DHT dht(tempSensor, sensorType);

    void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ready!");
    loadFromFlash();

    dht.begin();
    pinMode(fanPin, OUTPUT);

    Serial.println("Starting WiFiManager..");
    WiFiManager wifiManager;
    wifiManager.setBreakAfterConfig(true);
    wifiManager.setTimeout(300);
    if (!wifiManager.autoConnect("FanControl")) {
      Serial.println("failed to connect!");
      wifiManager.resetSettings();
      delay(2500);
      ESP.reset();
    }
    Serial.println("Connected to the network.");
    Serial.println(WiFi.localIP());
    MDNS.begin("fancontrol");
    Serial.println("Also accessible through http://fancontrol.local");

    timeClient.begin();
    timeClient.update();

    server.on("/", []() {
      server.send_P(200, "text/html", PAGE);
    });

    server.on("/data", []() {
      String json = "{\"temp\":" + String(currentTemp, 1);
      json += ",\"fanOn\":" + String(fanOn ? "true" : "false");
      json += ",\"onTemp\":" + String(onTemp, 1);
      json += ",\"utcOffset\":" + String(utcOffset / 3600);
      json += ",\"history\":[";
      int start = (historyIndex - historyCount + tempHistorySize) % tempHistorySize;
      for (int i = 0; i < historyCount; i++) {
        if (i > 0) json += ",";
        json += String(tempHistory[(start + i) % tempHistorySize], 1);
      }
      json += "]}";
      server.send(200, "application/json", json);
    });

    server.on("/saveAll", []() {
      float newTemp = server.arg("ontemp").toFloat();
      if (newTemp > 0 && newTemp < 100){
        onTemp = newTemp;
        offTemp = newTemp - 2.0;
        
      }

      int offset = server.arg("offset").toInt();
      if(offset >= -43200 && offset <= 50400) {
        utcOffset = offset;
        timeClient.setTimeOffset(utcOffset);
        saveToFlash();
      }

      saveToFlash();
      server.send(200, "text/plain", "ok");
    });

    server.on("/daily", []() {
      String json = "{\"count\":" + String(dailyCount) + ",\"data\":[";
      int start = (dailyIndex - dailyCount + dailyHistorySize) % dailyHistorySize;
      for(int i = 0; i < dailyCount; i++) {
        if(i > 0) json += ",";
        int idx = (start + i) % dailyHistorySize;
        time_t epoch = dailyTimes[idx];
        struct tm* t = localtime(&epoch);
        char timebuf[6];
        sprintf(timebuf, "%02d:%02d", t->tm_hour, t->tm_min);
        json += "{\"time\":\"" + String(timebuf) + "\",\"temp\":" + String(dailyTemps[idx], 1) + "}";
      }
      json += "]}";
      server.send(200, "application/json", json);
    });

    server.on("/export", []() {
      String csv = "Time,Temperature (C)\n";
      int start = (dailyIndex - dailyCount + dailyHistorySize) % dailyHistorySize;
      for (int i = 0; i < dailyCount; i++) {
        int idx = (start + i) % dailyHistorySize;
        time_t epoch = dailyTimes[idx];
        struct tm* t = localtime(&epoch);
        char timebuf[6];
        sprintf(timebuf, "%02d:%02d", t->tm_hour, t->tm_min);
        csv += String(timebuf) + "," + String(dailyTemps[idx], 1) + "\n";
      }
      server.sendHeader("Content-Disposition", "attachment; filename=Temperatures.csv");
      server.send(200, "text/csv", csv);
    });


    server.begin();
    Serial.println("Well, at least the webpage init was successful. (check prints from WiFiManager the page doesn't open.)");
  }

  void loop() {

    MDNS.update();
    server.handleClient();

    if (millis() - lastRead > 3000) {
      lastRead = millis();
      float t = dht.readTemperature();
      if (!isnan(t)) {
        currentTemp = t;
        tempHistory[historyIndex] = t;
        historyIndex = (historyIndex + 1) % tempHistorySize;
        if(historyCount < tempHistorySize) historyCount++;
        updFanStat(t);
        timeClient.update();
          // Serial.print("Temp: "); Serial.print(t); Serial.print("C");
          // Serial.println();
          // Serial.print("Fan: "); Serial.print(fanOn);
          // Serial.println();
          // This is commented out so the serial monitor doesn't get spammed. If something's not working, uncomment these prints.
      }
    else {
      Serial.println("Failed to get a reading from the DHT11, please ensure that your sensor is connected and check if your wiring is correct!");
    }
  }
  if (millis() - lastDailyRead > 300000) {
    lastDailyRead = millis();
    dailyTemps[dailyIndex] = currentTemp;
    dailyTimes[dailyIndex] = timeClient.getEpochTime();
    dailyIndex = (dailyIndex + 1) % dailyHistorySize;
    if(dailyCount < dailyHistorySize) dailyCount++;
  }
  MidnightReset();
}
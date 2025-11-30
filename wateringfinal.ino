#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>


const char* ssid = "xxxxo";
const char* password = "12345678";

const String serverURL = "http://10.90.128.97/elect/watering_system_db.php";


WebServer server(80);

const int analogPin = 32; 
const int pumpPin    = 26; 

const int PUMP_ON  = HIGH;
const int PUMP_OFF = LOW;

const unsigned long PUMP_DURATION_MS = 3000UL;
const unsigned long MIN_INTERVAL_MS  = 10000UL;
const unsigned long LOG_INTERVAL_MS  = 10000UL; 

unsigned long pumpStart = 0;
unsigned long lastPump  = 0;
unsigned long lastSend  = 0;

bool pumpRunning = false;
bool isDry = false;
bool systemEnabled = true;
int wateringCount = 0;


const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Automatic Yan</title>
<style>
:root {
    --bg: #FCFCF9;
    --surface: #FFFEFD;
    --muted: #667074;
    --text: #13343B;
    --primary: #21808D;
    --primary-600: #1D747F;
    --card-border: rgba(64, 77, 94, 0.12);
    --card-border-2: rgba(120, 94, 54, 0.08);
    --radius: 12px;
    --shadow: 0 4px 12px rgba(0,0,0,0.05);
}
@media (prefers-color-scheme: dark) {
    :root {
        --bg: #4be1e1;
        --bg2:rgb(0, 0, 0);
        --surface: #262828;
        --muted: #A7A9A9;
        --text: #F5F5F5;
        --primary: #32B8C6;
        --primary-600: #1EA7B3;
        --card-border: rgba(119,124,124,0.2);
        --card-border-2: rgba(119,124,124,0.12);
    }
}
* { box-sizing: border-box; margin: 0; padding: 0; }
html,body { height: 100%; }
body {
    background: linear-gradient(var(--bg), var(--bg2));
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 24px;
}
.container {
    width: 100%;
    max-width: 520px;
    background: var(--surface);
    border: 1px solid var(--card-border);
    border-radius: 16px;
    padding: 28px;
    box-shadow: var(--shadow);
}
h1 {
    font-size: 24px;
    margin-bottom: 6px;
    text-align: center;
}
.subtitle {
    text-align: center;
    color: var(--muted);
    font-size: 13px;
    margin-bottom: 20px;
}
.section {
    background: transparent;
    margin-bottom: 18px;
}
.welcome {
    text-align: center;
    padding: 12px;
    border-radius: var(--radius);
    border: 1px solid var(--card-border-2);
    background: transparent;
    margin-bottom: 16px;
}
.welcome .date {
    color: var(--muted);
    font-size: 13px;
}
.controls {
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 12px;
    margin-bottom: 12px;
}
.status-display {
    text-align: center;
    color: var(--muted);
    font-size: 14px;
}
.info-card {
    border-radius: 12px;
    padding: 16px;
    border: 1px solid var(--card-border-2);
    background: transparent;
    text-align: center;
}
.info-card label {
    display:block;
    font-size: 12px;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 0.6px;
    margin-bottom: 8px;
}
.info-value {
    font-size: 20px;
    font-weight: 600;
    color: var(--text);
}
.indicators {
    display:flex;
    align-items:center;
    justify-content:center;
    gap:10px;
    margin-top: 8px;
}
.dot {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: var(--card-border-2);
}
.dot.active {
    background: var(--primary);
    box-shadow: 0 0 10px rgba(33,128,141,0.14);
}
.button {
    display: inline-block;
    padding: 10px 16px;
    border-radius: 10px;
    background: var(--primary);
    color: #fff;
    border: none;
    cursor: pointer;
    font-weight: 600;
    transition: background 0.15s;
}
.button.secondary {
    background: transparent;
    color: var(--primary);
    border: 1px solid var(--card-border);
}
.button:active { transform: translateY(1px); }
.log {
    margin-top: 12px;
    max-height: 70px;
    overflow-y: auto;
    border-radius: 10px;
    padding: 10px;
    border: 1px solid var(--card-border-2);
    background: transparent;
}
.log-entry {
    display:flex;
    justify-content:space-between;
    padding: 8px;
    border-radius: 8px;
    background: var(--surface);
    margin-bottom: 8px;
    font-size: 13px;
}

/* Responsive */
@media (max-width:420px) {
    .container { padding: 18px; }
    .info-value { font-size:18px; }
}
</style>
</head>
<body>
<div class="container">
  <h1>Water Irrigation</h1>
  <p class="subtitle">Automatic Plant Watering System</p>

  <div class="welcome">
    <div class="welcome-text">Today is <span id="welcomeDateTime"></span></div>
    <div class="date" id="welcomeDay"></div>
  </div>

  <div class="section controls">
    <div>
      <div class="status-label"></div>
      <button id="toggleSystemBtn" class="button secondary" onclick="toggleSystem()">Toggle System</button>
    </div>
    <div style="min-width:160px;">
      <div class="status-display" id="systemStatus">System ON</div>
    </div>
  </div>

  <div class="section info-card">
    <label>Plant Condition</label>
    <div class="indicators">
      <div class="dot" id="dryDot"></div>
      <div class="info-value" id="moistureStatus">Checking...</div>
      <div class="dot" id="wetDot"></div>
    </div>
    <div style="margin-top:12px;">
      <button id="waterNowBtn" class="button">Water Now</button>
    </div>
    <div style="margin-top:8px;font-size:12px;color:var(--muted);" id="sensorUpdate">Last checked: --:--:--</div>
  </div>

  <div class="section log">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px;">
      <div style="font-weight:600;font-size:13px;color:var(--muted);">Status Log</div>
      <div style="font-size:12px;color:var(--muted);" id="logCount">0</div>
    </div>
    <div id="logContainer">
    </div>
  </div>
</div>

<script>
function formatTime(d){
  return d.toLocaleTimeString();
}
function updateWelcome() {
  const d = new Date();
  document.getElementById('welcomeDateTime').textContent = d.toLocaleDateString() + ' ' + d.toLocaleTimeString();
  document.getElementById('welcomeDay').textContent = d.toLocaleString(undefined, { weekday: 'long' });
}
setInterval(updateWelcome, 1000);
updateWelcome();

let systemOn = true;

async function fetchData(){
  try {
    const res = await fetch('/command?action=status');
    const data = await res.json();

    const moistureEl = document.getElementById('moistureStatus');
    const dryDot = document.getElementById('dryDot');
    const wetDot = document.getElementById('wetDot');
    const sensorUpdate = document.getElementById('sensorUpdate');

    if(data.moisture === 'dry') {
      moistureEl.textContent = 'DRY (' + data.moisturePercent + '%)';
      dryDot.classList.add('active');
      wetDot.classList.remove('active');
    } else {
      moistureEl.textContent = 'WET (' + data.moisturePercent + '%)';
      dryDot.classList.remove('active');
      wetDot.classList.add('active');
    }

    sensorUpdate.textContent = 'Last checked: ' + formatTime(new Date());

    // add to log
    const log = document.getElementById('logContainer');
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerHTML = '<div>' + new Date().toLocaleTimeString() + ' — ' + data.moisture.toUpperCase() + '</div><div>' + data.moisturePercent + '%</div>';
    log.prepend(entry);

    // keep log count display
    const count = log.childElementCount;
    document.getElementById('logCount').textContent = count;
    // limit entries
    while (log.childElementCount > 20) log.removeChild(log.lastChild);

    // update system status display
    document.getElementById('systemStatus').textContent = systemOn ? 'System ON' : 'System OFF';
  } catch (e) {
    console.error('Fetch error', e);
  }
}

setInterval(fetchData, 2000);
fetchData();

function toggleSystem(){
  systemOn = !systemOn;
  const btn = document.getElementById('toggleSystemBtn');
  if(systemOn){
    btn.classList.remove('secondary');
    btn.textContent = 'Disable System';
    fetch('/command?action=ON').catch(()=>{});
  } else {
    btn.classList.add('secondary');
    btn.textContent = 'Enable System';
    fetch('/command?action=OFF').catch(()=>{});
  }
  document.getElementById('systemStatus').textContent = systemOn ? 'System ON' : 'System OFF';
}

document.getElementById('waterNowBtn').addEventListener('click', function(){
  fetch('/command?action=WATER').catch(()=>{});
});
</script>
</body>
</html>
)rawliteral";


void logToDatabase(int moisturePercent, const String& moistureLevel, int count) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping DB log.");
    return;
  }

  HTTPClient http;
  http.begin("http://10.90.128.97/elect/watering_system_db.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String moistureStr = String(moisturePercent) + "%";
  String postData = "moisture=" + moistureStr +
                    "&moisture_level=" + moistureLevel +
                    "&count=" + String(count);

  int httpResponseCode = http.POST(postData);
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("HTTP %d : %s\n", httpResponseCode, response.c_str());
  } else {
    Serial.printf("HTTP POST failed, error: %d\n", httpResponseCode);
  }
  http.end();
}


void setup() {
  Serial.begin(115200);

  pinMode(analogPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, PUMP_OFF);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", htmlPage);
  });

  server.on("/command", HTTP_GET, []() {

    int raw = analogRead(analogPin);
    int moisturePercent = 100 - ((raw * 100) / 4095);
    moisturePercent = constrain(moisturePercent, 0, 100);

    isDry = (moisturePercent <= 35); 
    String moistureText = isDry ? "dry" : "wet";

    String action = server.arg("action");

    if(action == "WATER") startPump();
    else if(action == "ON") systemEnabled = true;
    else if(action == "OFF") { systemEnabled = false; stopPump(); }

    
    String json = "{";
    json += "\"moisture\":\"" + moistureText + "\",";
    json += "\"moisturePercent\":" + String(moisturePercent) + ",";
    json += "\"count\":" + String(wateringCount);
    json += "}";

    server.send(200, "application/json", json);
  });

  server.begin();

  lastSend = millis(); 
}

void loop() {
  server.handleClient();

  int raw = analogRead(analogPin);
  int moisturePercent = 100 - ((raw * 100) / 4095);
  moisturePercent = constrain(moisturePercent, 0, 100);


  isDry = (moisturePercent <= 35);

  unsigned long now = millis();

  if(systemEnabled && !pumpRunning && isDry && (now - lastPump >= MIN_INTERVAL_MS)) {
    startPump();
  }

  if(pumpRunning && (now - pumpStart >= PUMP_DURATION_MS)) stopPump();

  if (now - lastSend >= LOG_INTERVAL_MS) {
    String level = isDry ? "dry" : "wet";
    logToDatabase(moisturePercent, level, wateringCount);
    lastSend = now;
  }
}


void startPump() {
  pumpRunning = true;
  pumpStart = millis();
  lastPump = millis();
  digitalWrite(pumpPin, PUMP_ON);
  wateringCount++;
  Serial.println("Pump ON");

  int raw = analogRead(analogPin);
  int moisturePercent = 100 - ((raw * 100) / 4095);
  moisturePercent = constrain(moisturePercent, 0, 100);
  String level = (moisturePercent <= 35) ? "dry" : "wet";
  logToDatabase(moisturePercent, level, wateringCount);
}

void stopPump() {
  pumpRunning = false;
  digitalWrite(pumpPin, PUMP_OFF);
  Serial.println("Pump OFF");
}
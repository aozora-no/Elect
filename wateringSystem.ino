#include <WiFi.h>
#include <WebServer.h>

// === WIFI SETTINGS ===
const char* ssid = "Reconnecting....";   
const char* password = "https://Nuqui "; 

// === WEB SERVER ===
WebServer server(80);

// === PINS ===
const int analogPin = 32;   // Soil AO
const int digitalPin = 25;  // Soil DO
const int pumpPin    = 26;  // Relay IN

const int PUMP_ON  = HIGH;
const int PUMP_OFF = LOW;

const unsigned long PUMP_DURATION_MS = 3000UL;
const unsigned long MIN_INTERVAL_MS  = 10000UL; // 1 min gap

unsigned long pumpStart = 0;
unsigned long lastPump  = 0;
bool pumpRunning = false;

bool isDry = true;
int wateringCount = 0;

// === SYSTEM CONTROL FLAG ===
bool systemEnabled = true;

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Automatic Water Dispenser</title>
<style>
:root {
    --color-cream-50: rgba(252, 252, 249, 1);
    --color-cream-100: rgba(255, 255, 253, 1);
    --color-gray-300: rgba(167, 169, 169, 1);
    --color-slate-500: rgba(98, 108, 113, 1);
    --color-charcoal-700: rgba(31, 33, 33, 1);
    --color-charcoal-800: rgba(38, 40, 40, 1);
    --color-teal-300: rgba(50, 184, 198, 1);
    --color-teal-500: rgba(33, 128, 141, 1);
    --color-teal-600: rgba(29, 116, 128, 1);
    --color-brown-600-rgb: 94, 82, 64;
    --color-background: var(--color-cream-50);
    --color-surface: var(--color-cream-100);
    --color-text: rgba(19, 52, 59, 1);
    --color-text-secondary: var(--color-slate-500);
    --color-primary: var(--color-teal-500);
    --color-primary-hover: var(--color-teal-600);
    --color-border: rgba(var(--color-brown-600-rgb), 0.2);
    --color-card-border: rgba(var(--color-brown-600-rgb), 0.12);
}
@media (prefers-color-scheme: dark) {
    :root {
        --color-background: var(--color-charcoal-700);
        --color-surface: var(--color-charcoal-800);
        --color-text: rgba(245, 245, 245, 1);
        --color-text-secondary: rgba(167, 169, 169, 0.7);
        --color-primary: var(--color-teal-300);
        --color-border: rgba(119, 124, 124, 0.3);
        --color-card-border: rgba(119, 124, 124, 0.2);
    }
}
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family:-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: var(--color-background); color: var(--color-text); padding:20px; min-height:100vh; display:flex; justify-content:center; align-items:center; }
.container { max-width:500px; width:100%; background:var(--color-surface); border-radius:16px; border:1px solid var(--color-card-border); padding:32px; box-shadow:0 4px 12px rgba(0,0,0,0.05); }
h1 { font-size:28px; font-weight:600; margin-bottom:8px; text-align:center; }
.subtitle { text-align:center; color:var(--color-text-secondary); font-size:14px; margin-bottom:24px; }
.welcome-section { text-align:center; padding:16px; background:var(--color-background); border:1px solid var(--color-card-border); border-radius:12px; margin-bottom:24px; }
.welcome-text { font-size:14px; color:var(--color-text); margin-bottom:8px; line-height:1.5; }
.welcome-day { font-size:18px; font-weight:600; color:var(--color-primary); }
.control-section { margin-bottom:32px; }
.toggle-container { display:flex; justify-content:center; align-items:center; gap:16px; margin-bottom:12px; }
.status-label { font-size:18px; font-weight:500; }
.toggle-switch { position:relative; width:72px; height:40px; background:var(--color-border); border-radius:40px; cursor:pointer; transition:background-color 0.3s; }
.toggle-switch.active { background:var(--color-primary); }
.toggle-slider { position:absolute; top:4px; left:4px; width:32px; height:32px; background:white; border-radius:50%; transition:transform 0.3s; box-shadow:0 2px 4px rgba(0,0,0,0.2); }
.toggle-switch.active .toggle-slider { transform:translateX(32px); }
.status-display { text-align:center; font-size:16px; color:var(--color-text-secondary); }
.info-grid { display:grid; grid-template-columns:1fr; gap:16px; margin-bottom:24px; }
.info-card { background:var(--color-background); border:1px solid var(--color-card-border); border-radius:12px; padding:20px; text-align:center; }
.info-card label { display:block; font-size:13px; color:var(--color-text-secondary); margin-bottom:8px; text-transform:uppercase; letter-spacing:0.5px; }
.info-card .value { font-size:22px; font-weight:600; color:var(--color-text); }
.info-card.full-width { grid-column:1/-1; }
.moisture-indicator { display:flex; align-items:center; justify-content:center; gap:8px; }
.moisture-dot { width:12px; height:12px; border-radius:50%; background:var(--color-border); }
.moisture-dot.active { background:var(--color-primary); box-shadow:0 0 12px var(--color-primary); }
.watering-count { text-align:center; padding:24px; background:var(--color-background); border:1px solid var(--color-card-border); border-radius:12px; }
.watering-count label { display:block; font-size:13px; color:var(--color-text-secondary); margin-bottom:8px; text-transform:uppercase; letter-spacing:0.5px; }
.watering-count .count { font-size:48px; font-weight:700; color:var(--color-primary); }
.timestamp { font-size:11px; color:var(--color-text-secondary); margin-top:4px; }
@keyframes pulse {0%,100% {opacity:1;}50% {opacity:0.5;}}
.active-pulse {animation:pulse 1.5s ease-in-out infinite;}
.status-log { background:var(--color-background); border:1px solid var(--color-card-border); border-radius:12px; padding:16px; max-height:200px; overflow-y:auto; }
.status-log label { display:block; font-size:13px; color:var(--color-text-secondary); margin-bottom:12px; text-transform:uppercase; letter-spacing:0.5px; font-weight:600; }
.log-entry { display:flex; justify-content:space-between; align-items:center; padding:8px 12px; background:var(--color-surface); border-radius:6px; margin-bottom:8px; font-size:13px; }
.log-entry:last-child { margin-bottom:0; }
.log-date { color:var(--color-text); font-weight:500; }
.log-time { color:var(--color-text-secondary); margin-left:8px; }
.log-count { color:var(--color-primary); font-weight:600; }
</style>
</head>
<body>
<div class="container">
<h1>💧 Water Dispenser</h1>
<p class="subtitle">Automatic Plant Watering System</p>
<div class="welcome-section">
<div class="welcome-text">Welcome back, Today is <span id="welcomeDateTime"></span></div>
<div class="welcome-day" id="welcomeDay"></div>
</div>

<div class="control-section">
<div class="toggle-container">
<span class="status-label">System</span>
<div class="toggle-switch" id="toggleSwitch" onclick="toggleSystem()">
<div class="toggle-slider"></div>
</div>
</div>
<div class="status-display" id="statusDisplay">System OFF</div>
</div>

<div class="info-grid">
<div class="info-card full-width">
<label>Soil Condition</label>
<div class="moisture-indicator">
<div class="moisture-dot" id="dryIndicator"></div>
<div class="value" id="moistureStatus">Checking...</div>
<div class="moisture-dot" id="wetIndicator"></div>
</div>
<div class="timestamp" id="sensorUpdate">Last checked: --:--:--</div>
</div>
</div>

<div class="status-log">
<label>Status Log</label>
<div id="statusLogContainer"></div>
</div>

<div class="watering-count">
<label>Times Watered Today</label>
<div class="count" id="waterCount">0</div>
<div class="timestamp" id="lastWatered">Last watered: Never</div>
</div>
</div>

<script>
let systemOn = true;

async function fetchData() {
    const res = await fetch('/command?action=status');
    const data = await res.json();
    const moistureStatus = document.getElementById('moistureStatus');
    const dryDot = document.getElementById('dryIndicator');
    const wetDot = document.getElementById('wetIndicator');
    const waterCount = document.getElementById('waterCount');

    if(data.moisture === 'dry') {
        moistureStatus.textContent = 'DRY';
        dryDot.classList.add('active');
        wetDot.classList.remove('active');
    } else {
        moistureStatus.textContent = 'WET';
        dryDot.classList.remove('active');
        wetDot.classList.add('active');
    }
    waterCount.textContent = data.count;
}

setInterval(fetchData, 2000);

function toggleSystem() {
    systemOn = !systemOn;
    const toggleSwitch = document.getElementById('toggleSwitch');
    const statusDisplay = document.getElementById('statusDisplay');
    if(systemOn){
        toggleSwitch.classList.add('active');
        statusDisplay.textContent = 'System ON';
        fetch('/command?action=ON');
    } else {
        toggleSwitch.classList.remove('active');
        statusDisplay.textContent = 'System OFF';
        fetch('/command?action=OFF');
    }
}
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(analogPin, INPUT);
  pinMode(digitalPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, PUMP_OFF);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Serve the HTML page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage);
  });

  // Command endpoint
 // Inside the /command endpoint
server.on("/command", HTTP_GET, []() {
  String action = server.arg("action");

  if(action == "ON") {
    systemEnabled = true;   // Enable system
    startPump();            // Force pump ON immediately
  } 
  else if(action == "OFF") {
    systemEnabled = false;  // Disable system
    stopPump();             // Stop pump immediately
  } 
  else if(action == "WATER") {
    startPump();
  }

  server.send(200, "application/json",
              String("{\"moisture\":\"") + (isDry ? "dry" : "wet") +
              String("\",\"count\":") + wateringCount + "}");
});

  server.begin();
}

void loop() {
  server.handleClient();

  int soilDO = digitalRead(digitalPin);
  isDry = (soilDO == HIGH);

  unsigned long now = millis();

  // Auto water ONLY if systemEnabled
  if (systemEnabled && !pumpRunning && isDry && (now - lastPump >= MIN_INTERVAL_MS)) {
    startPump();
  }

  if (pumpRunning && (now - pumpStart >= PUMP_DURATION_MS)) {
    stopPump();
  }
}

void startPump() {
  pumpRunning = true;
  pumpStart = millis();
  lastPump = millis();
  digitalWrite(pumpPin, PUMP_ON);
  wateringCount++;
  Serial.println("Pump ON");
}

void stopPump() {
  pumpRunning = false;
  digitalWrite(pumpPin, PUMP_OFF);
  Serial.println("Pump OFF");
}

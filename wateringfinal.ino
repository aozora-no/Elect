#include <WiFi.h>
#include <WebServer.h>

// === WIFI SETTINGS ===
const char* ssid = "Reconnecting....";
const char* password = "https://Nuqui ";

// === WEB SERVER ===
WebServer server(80);

// === PINS ===
const int analogPin = 32;   // Soil sensor AO
const int pumpPin    = 26;  // Relay IN

const int PUMP_ON  = HIGH;
const int PUMP_OFF = LOW;

const unsigned long PUMP_DURATION_MS = 3000UL;
const unsigned long MIN_INTERVAL_MS  = 10000UL;

unsigned long pumpStart = 0;
unsigned long lastPump  = 0;

bool pumpRunning = false;
bool isDry = false;
bool systemEnabled = true;
int wateringCount = 0;

// ================================
// HTML PAGE
// ================================
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Water Dispenser</title>
<style>
/* Keep your CSS */
</style>
</head>
<body>
<h2>Soil Status</h2>
<p>Moisture: <span id="moisturePercent">0</span>%</p>
<p>Status: <span id="moistureStatus">Checking...</span></p>

<button onclick="fetch('/command?action=WATER')">WATER NOW</button>

<script>
async function fetchData() {
    const res = await fetch('/command?action=status');
    const data = await res.json();

    document.getElementById("moisturePercent").textContent = data.moisturePercent;
    document.getElementById("moistureStatus").textContent = data.moisture;

}
setInterval(fetchData, 2000);
</script>
</body>
</html>
)rawliteral";

// ================================
// BACKEND
// ================================
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

  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", htmlPage);
  });

  server.on("/command", HTTP_GET, []() {

    int raw = analogRead(analogPin);
    int moisturePercent = 100 - ((raw * 100) / 4095);
    moisturePercent = constrain(moisturePercent, 0, 100);

    // ==========================
    // ✔ CORRECT DRY/WET LOGIC
    // ==========================
    isDry = (moisturePercent <= 35);   // DRY at <= 35%
    String moistureText = isDry ? "dry" : "wet";

    String action = server.arg("action");

    if(action == "WATER") startPump();
    else if(action == "ON") systemEnabled = true;
    else if(action == "OFF") { systemEnabled = false; stopPump(); }

    // JSON response
    String json = "{";
    json += "\"moisture\":\"" + moistureText + "\",";
    json += "\"moisturePercent\":" + String(moisturePercent) + ",";
    json += "\"count\":" + String(wateringCount);
    json += "}";

    server.send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  server.handleClient();

  int raw = analogRead(analogPin);
  int moisturePercent = 100 - ((raw * 100) / 4095);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // ==========================
  // ✔ CORRECT DRY/WET LOGIC
  // ==========================
  isDry = (moisturePercent <= 35); // DRY at <= 35%

  unsigned long now = millis();

  if(systemEnabled && !pumpRunning && isDry && (now - lastPump >= MIN_INTERVAL_MS)) {
    startPump();
  }

  if(pumpRunning && (now - pumpStart >= PUMP_DURATION_MS)) stopPump();
}

// ================================
// PUMP CONTROL
// ================================
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

#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// --- User config: set your SSID, password and server URL here ---
const char* ssid = "Reconnecting....";
const char* password = "https://Nuqui ";
const char* serverName = "http://192.168.1.3/elect_final/catconnect.php";
// ---------------------------------------------------------------

#define HX711_DT   27
#define HX711_SCK  26
#define SERVO_PIN  13

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

HX711 scale;
Servo myservo;
WebServer server(80);

// Calibrate to your scale. If readings are reversed, invert the sign.
float calibration_factor = -2280.0;

// Activate servo automatically when grams < lowFoodThreshold
float lowFoodThreshold = 60.0;

float currentWeight = 0;
bool autoDispenseEnabled = false;

// Throttle periodic updates to the server to avoid spamming (ms)
unsigned long lastSendMillis = 0;
const unsigned long sendInterval = 10000UL; // 10s

// Servo angle limits (0-180). Use these to ensure we never command out-of-range angles.
const int SERVO_MIN_ANGLE = 0;
const int SERVO_MAX_ANGLE = 180;

// Rest and dispense angles (must be within SERVO_MIN_ANGLE..SERVO_MAX_ANGLE)
const int SERVO_REST_ANGLE = 90;      // neutral position
const int SERVO_DISPENSE_ANGLE = 160; // move to this to dispense (adjust as needed, must be 0..180)

// Pulse width range used for attach() (microseconds). Adjust for your servo if needed.
const int SERVO_MIN_PULSE_US = 500;
const int SERVO_MAX_PULSE_US = 2400;

// Auto-dispense state machine variables (non-blocking)
bool dispensingActive = false;        // true while auto-dispensing cycles are running
uint8_t dispenseState = 0;            // 0=idle, 1=moved out, 2=holding out, 3=moved back (rest)
unsigned long dispenseLastMillis = 0; // timestamp of last state change

// Durations for each state (ms)
const unsigned long HOLD_OUT_MS = 600;   // how long to hold at dispense angle
const unsigned long HOLD_BACK_MS = 400;  // how long to hold at rest between cycles
const float HYSTERESIS = 8.0;            // grams above threshold to stop auto-dispense

// Send DB only when starting/stopping auto-dispense or on manual dispense
bool autoReportedAsDispensed = false;

// Track attachment state to detach when idle (stop holding torque / noise)
bool servoAttached = false;

void attachServo() {
  if (!servoAttached) {
    myservo.attach(SERVO_PIN, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    servoAttached = true;
    // small settle delay can help hardware
    delay(20);
  }
}

void detachServo() {
  if (servoAttached) {
    myservo.detach();
    servoAttached = false;
    // short delay to allow motor to stop drawing/sounding
    delay(5);
  }
}

void sendToDatabase(const String &feedingType, bool dispensed) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi; skipping HTTP POST");
    return;
  }

  HTTPClient http;
  if (!http.begin(serverName)) {
    Serial.println("HTTP begin failed");
    return;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String statusStr = (currentWeight <= lowFoodThreshold) ? "low" : "full";

  String postData =
    "weight=" + String(currentWeight, 1) +
    "&status=" + statusStr +
    "&feeding=" + feedingType +
    "&dispensed=" + String(dispensed ? 1 : 0);

  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.printf("POST %s -> code: %d, resp: %s\n", serverName, httpCode, payload.c_str());
  } else {
    Serial.printf("POST failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void handleData() {
  // Return JSON with fields expected by the front-end exactly:
  // weight (number), status (string), lowThreshold (number), autoEnabled (boolean)
  String statusStr = (currentWeight <= lowFoodThreshold) ? "low" : "full";
  String json = "{";
  json += "\"weight\":" + String(currentWeight, 1) + ",";
  json += "\"status\":\"" + statusStr + "\",";
  json += "\"lowThreshold\":" + String(lowFoodThreshold, 1) + ",";
  json += "\"autoEnabled\":" + String(autoDispenseEnabled ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  // Serve the exact HTML provided by the user. Do NOT modify the HTML content.
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cat Food Dispenser</title>
    <style>
        :root {
            --primary-color: #FF6B9D;
            --secondary-color: #FFC75F;
            --accent-color: #845EC2;
            --text-color: #2D2E32;
            --bg-light: #F8F7F3;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, rgba(255, 107, 157, 0.1) 0%, rgba(132, 94, 194, 0.1) 100%),
                        url('https://user-gen-media-assets.s3.amazonaws.com/seedream_images/189c0988-6fc6-4b51-9ab1-131c984a6da8.png') center/cover no-repeat fixed;
            background-blend-mode: screen;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
            color: var(--text-color);
        }

        .container {
            background: rgba(248, 247, 243, 0.95);
            border-radius: 30px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.15);
            backdrop-filter: blur(10px);
            border: 2px solid rgba(255, 107, 157, 0.2);
        }

        h1 {
            text-align: center;
            color: var(--primary-color);
            margin-bottom: 10px;
            font-size: 32px;
            text-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }

        .subtitle {
            text-align: center;
            color: var(--accent-color);
            margin-bottom: 30px;
            font-size: 14px;
            font-weight: 500;
        }

        .status-section {
            background: linear-gradient(135deg, rgba(255, 107, 157, 0.1) 0%, rgba(255, 199, 95, 0.1) 100%);
            border-radius: 20px;
            padding: 25px;
            margin-bottom: 30px;
            border-left: 4px solid var(--primary-color);
            text-align: center;
        }

        .weight-display {
            font-size: 48px;
            font-weight: bold;
            color: var(--primary-color);
            margin: 15px 0;
            font-family: 'Courier New', monospace;
        }

        .status-label {
            font-size: 14px;
            color: var(--text-color);
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }

        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
            animation: pulse 2s infinite;
        }

        .status-indicator.good {
            background-color: #2ECC71;
        }

        .status-indicator.low {
            background-color: #FF6B6B;
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .controls-section {
            margin-bottom: 25px;
        }

        .control-label {
            display: block;
            font-size: 14px;
            font-weight: 600;
            margin-bottom: 12px;
            color: var(--accent-color);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .button-group {
            display: grid;
            grid-template-columns: 1fr;
            gap: 12px;
            margin-bottom: 20px;
        }

        .btn {
            padding: 14px 20px;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
        }

        .btn-dispense {
            background: linear-gradient(135deg, var(--primary-color) 0%, #FF4081 100%);
            color: white;
            grid-column: 1 / -1;
        }

        .btn-dispense:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(255, 107, 157, 0.4);
        }

        .btn-dispense:active {
            transform: translateY(0);
            box-shadow: 0 2px 10px rgba(255, 107, 157, 0.4);
        }

        .btn-toggle {
            background: linear-gradient(135deg, var(--secondary-color) 0%, #FFB84D 100%);
            color: white;
        }

        .btn-toggle:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(255, 199, 95, 0.4);
        }

        .btn-toggle:active {
            transform: translateY(0);
        }

        .btn-toggle.active {
            background: linear-gradient(135deg, #2ECC71 0%, #27AE60 100%);
        }

        .status-display {
            background: linear-gradient(135deg, rgba(132, 94, 194, 0.1) 0%, rgba(46, 204, 113, 0.1) 100%);
            padding: 16px;
            border-radius: 12px;
            text-align: center;
            margin-top: 15px;
            border: 1px solid rgba(132, 94, 194, 0.2);
        }

        .status-text {
            font-size: 14px;
            color: var(--text-color);
            margin-bottom: 8px;
        }

        .mode-indicator {
            display: inline-block;
            padding: 6px 14px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
            background: rgba(255, 107, 157, 0.2);
            color: var(--primary-color);
        }

        .mode-indicator.active {
            background: rgba(46, 204, 113, 0.2);
            color: #2ECC71;
        }

        .info-section {
            background: rgba(255, 199, 95, 0.1);
            padding: 16px;
            border-radius: 12px;
            border-left: 4px solid var(--secondary-color);
            margin-top: 25px;
            font-size: 13px;
            color: var(--text-color);
            line-height: 1.6;
            text-align: center;
        }

        .info-section strong {
            color: var(--accent-color);
        }

        .loading {
            display: inline-block;
            width: 12px;
            height: 12px;
            border: 2px solid rgba(132, 94, 194, 0.3);
            border-radius: 50%;
            border-top-color: var(--accent-color);
            animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        @media (max-width: 480px) {
            .container {
                padding: 30px 20px;
            }

            h1 {
                font-size: 26px;
            }

            .weight-display {
                font-size: 40px;
            }

            .button-group {
                gap: 10px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐱 Pet Feeder</h1>
        <p class="subtitle">Smart Pet Food Dispenser</p>

        <div class="status-section">
            <div class="status-label">
                <span class="status-indicator good" id="statusIndicator"></span>
                Food Level
            </div>
            <div class="weight-display" id="weightDisplay">-- g</div>
            <div class="status-text" id="statusText">Loading...</div>
        </div>

        <div class="controls-section">
            <label class="control-label">Dispense Food</label>
            <div class="button-group">
                <button class="btn btn-dispense" onclick="dispensFood()">
                    <span id="dispenseText">🍖 Dispense Now</span>
                </button>
            </div>

            <label class="control-label">Auto Mode</label>
            <div class="button-group">
                <button class="btn btn-toggle" id="toggleBtn" onclick="toggleAutoMode()">
                    <span id="toggleText">Enable Auto</span>
                </button>
            </div>

            <div class="status-display">
                <div class="status-text">Auto Mode Status</div>
                <div class="mode-indicator" id="modeIndicator">OFF</div>
            </div>
        </div>

        <div class="info-section">
            <strong>👥 Members:</strong><br>
            • Nuqui, John Benedihh🥀🥀🥀 P.<br>
            • Santos, Kenzo S.<br>
            • Tolentino, Jastien Myer Q.<br>
            • Tolentino, Quirsten Louis T.
        </div>
    </div>

    <script>
        const API_BASE = 'http://' + window.location.hostname;

        async function fetchData() {
            try {
                const response = await fetch(API_BASE + '/data');
                const data = await response.json();

                document.getElementById('weightDisplay').textContent =
                    data.weight.toFixed(1) + ' g';

                const indicator = document.getElementById('statusIndicator');
                const statusText = document.getElementById('statusText');

                if (data.weight <= data.lowThreshold) {
                    indicator.classList.remove('good');
                    indicator.classList.add('low');
                    statusText.textContent = '⚠️ Food is LOW - Refill Soon!';
                } else {
                    indicator.classList.remove('low');
                    indicator.classList.add('good');
                    statusText.textContent = '✓ Food level is GOOD';
                }

                updateModeDisplay(data.autoEnabled);
            } catch (error) {
                console.error(error);
                document.getElementById('weightDisplay').textContent = 'Error';
                document.getElementById('statusText').textContent = 'Connection failed';
            }
        }

        async function dispensFood() {
            const btn = event.target.closest('.btn-dispense');
            const originalText = btn.innerHTML;

            btn.innerHTML = '<span class="loading"></span> Dispensing...';
            btn.disabled = true;

            try {
                const response = await fetch(API_BASE + '/dispense');
                if (response.ok) {
                    btn.innerHTML = '✓ Dispensed!';
                    setTimeout(() => {
                        btn.innerHTML = originalText;
                        btn.disabled = false;
                        fetchData();
                    }, 2000);
                }
            } catch (error) {
                console.error(error);
                btn.innerHTML = '✗ Failed';
                setTimeout(() => {
                    btn.innerHTML = originalText;
                    btn.disabled = false;
                }, 2000);
            }
        }

        async function toggleAutoMode() {
            const btn = document.getElementById('toggleBtn');
            btn.disabled = true;

            try {
                const response = await fetch(API_BASE + '/toggle');
                const status = await response.text();
                updateModeDisplay(status === 'ON');
                btn.disabled = false;
                fetchData();
            } catch (error) {
                console.error(error);
                btn.disabled = false;
            }
        }

        function updateModeDisplay(isEnabled) {
            const indicator = document.getElementById('modeIndicator');
            const btn = document.getElementById('toggleBtn');

            if (isEnabled) {
                indicator.textContent = 'ON';
                indicator.classList.add('active');
                btn.textContent = '✓ Auto Mode ON';
                btn.classList.add('active');
            } else {
                indicator.textContent = 'OFF';
                indicator.classList.remove('active');
                btn.textContent = '✗ Auto Mode OFF';
                btn.classList.remove('active');
            }
        }

        fetchData();
        setInterval(fetchData, 2000);
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleToggle() {
  autoDispenseEnabled = !autoDispenseEnabled;
  Serial.printf("Auto dispense toggled: %s\n", autoDispenseEnabled ? "ON" : "OFF");
  // If enabling auto and current weight is low, start dispensing right away
  if (autoDispenseEnabled && currentWeight <= lowFoodThreshold && !dispensingActive) {
    dispensingActive = true;
    dispenseState = 1; // start by moving out
    dispenseLastMillis = millis();
    autoReportedAsDispensed = false; // allow reporting once started
  }
  server.send(200, "text/plain", autoDispenseEnabled ? "ON" : "OFF");
}

// Manual single dispense (blocking as requested) — attach, move, detach
void performManualDispense(bool reportToDB) {
  int dispenseAngle = constrain(SERVO_DISPENSE_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  int restAngle    = constrain(SERVO_REST_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  attachServo();
  myservo.write(dispenseAngle);
  delay(400); // user requested behavior: short blocking movement
  myservo.write(restAngle);
  delay(120); // give servo time to reach rest
  detachServo();

  if (reportToDB) sendToDatabase("manual", true);
}

void handleDispense() {
  performManualDispense(true);
  server.send(200, "text/plain", "OK");
}

void startAutoDispense() {
  if (!dispensingActive) {
    dispensingActive = true;
    dispenseState = 1; // first state: move out
    dispenseLastMillis = millis();
    autoReportedAsDispensed = false;
    Serial.println("Auto-dispense STARTED");
  }
}

void stopAutoDispense() {
  if (dispensingActive) {
    dispensingActive = false;
    dispenseState = 0;
    // ensure servo goes to rest and detach to remove holding torque/sound
    attachServo();
    myservo.write(constrain(SERVO_REST_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
    delay(120);
    detachServo();
    // report stop (optional): send a status update
    sendToDatabase("auto", false);
    Serial.println("Auto-dispense STOPPED");
  } else {
    // also ensure servo is detached when auto disabled
    detachServo();
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // HX711
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(calibration_factor);
  scale.tare();
  if (!scale.is_ready()) {
    Serial.println("Warning: HX711 not ready!");
  } else {
    Serial.println("HX711 ready");
  }

  // Servo - attach briefly to move to rest, then detach to avoid idle sound
  myservo.setPeriodHertz(50);
  attachServo();
  myservo.write(constrain(SERVO_REST_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
  delay(150);
  detachServo();

  // Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
  }

  // Connect WiFi with timeout to avoid infinite loop
  Serial.printf("Connecting to WiFi SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  const unsigned long wifiTimeout = 20000UL; // 20 seconds
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < wifiTimeout) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed or timed out. Continuing without WiFi.");
  }

  // Web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/dispense", handleDispense);
  server.on("/toggle", handleToggle);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Read weight (average over 5 readings)
  float rawWeight = scale.get_units(5);
  // Ensure non-negative reading (adjust depending on your wiring/calibration)
  currentWeight = rawWeight >= 0 ? rawWeight : 0.0;

  // Auto mode control: start/stop dispensing based on weight
  if (autoDispenseEnabled) {
    if (currentWeight <= lowFoodThreshold) {
      startAutoDispense();
    } else if (dispensingActive && currentWeight > (lowFoodThreshold + HYSTERESIS)) {
      // stop once the weight recovers sufficiently above threshold
      stopAutoDispense();
    }
  } else {
    // if auto disabled, ensure auto-dispensing is stopped and servo detached
    if (dispensingActive) stopAutoDispense();
    else detachServo();
  }

  // Non-blocking state machine: run dispensing cycles while dispensingActive is true.
  if (dispensingActive) {
    unsigned long now = millis();
    switch (dispenseState) {
      case 1: // move out (start of cycle)
        attachServo();
        myservo.write(constrain(SERVO_DISPENSE_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
        dispenseLastMillis = now;
        dispenseState = 2;
        // report to DB once when starting the auto-dispense (avoid spamming)
        if (!autoReportedAsDispensed) {
          sendToDatabase("auto", true);
          autoReportedAsDispensed = true;
        }
        break;

      case 2: // holding out
        if (now - dispenseLastMillis >= HOLD_OUT_MS) {
          // move back to rest
          myservo.write(constrain(SERVO_REST_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
          dispenseLastMillis = now;
          dispenseState = 3;
        }
        break;

      case 3: // holding back between cycles
        if (now - dispenseLastMillis >= HOLD_BACK_MS) {
          // detach to stop holding torque (removes sound) while waiting to start next cycle
          detachServo();
          // start next cycle (move out again) only if still low
          if (currentWeight <= lowFoodThreshold) {
            dispenseState = 1;
          } else {
            // if weight no longer low, stop dispensing
            stopAutoDispense();
          }
        }
        break;

      default:
        // safety: go to rest and detach
        attachServo();
        myservo.write(constrain(SERVO_REST_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
        delay(50);
        detachServo();
        dispenseState = 0;
        break;
    }
  }

  // Update OLED display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  String statusStr = (currentWeight <= lowFoodThreshold) ? "LOW" : "FULL";
  if (currentWeight <= lowFoodThreshold) {
    display.setTextSize(2);
    display.println("LOW FOOD");
    display.setTextSize(1);
    display.print("W: ");
    display.print(currentWeight, 1);
    display.println(" g");
  } else {
    display.setTextSize(1);
    display.print("Weight: ");
    display.print(currentWeight, 1);
    display.println(" g");
  }
  display.display();

  // Periodic update to remote server for status (not every loop)
  if (WiFi.status() == WL_CONNECTED && (millis() - lastSendMillis) > sendInterval) {
    // send a "status" record (not a dispense) so DB has recent weight info
    sendToDatabase("status", false);
    lastSendMillis = millis();
  }

  // keep loop responsive
  delay(100);
}
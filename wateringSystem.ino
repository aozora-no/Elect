/*
   ESP32 Soil Moisture + Relay Pump System
   Force RELAY OFF = LOW to avoid unwanted activation.
   Works on relays unstable at 3.3V.
*/

const int analogPin = 32;   // Soil AO → GPIO32 (ADC1)
const int digitalPin = 25;  // Soil DO → GPIO25
const int pumpPin    = 26;  // Relay IN → GPIO26

const int PUMP_ON  = HIGH;   // Relay ON = HIGH
const int PUMP_OFF = LOW;    // Relay OFF = LOW (safer for ESP32)

const unsigned long PUMP_DURATION_MS = 3000UL;   // Pump runs 3 sec
const unsigned long MIN_INTERVAL_MS  = 10000UL;  // 1-minute cooldown

unsigned long pumpStart = 0;
unsigned long lastPump  = 0;
bool pumpRunning = false;

void setup() {
  Serial.begin(115200);

  // Force relay OFF at boot
  digitalWrite(pumpPin, PUMP_OFF);
  pinMode(pumpPin, OUTPUT);

  pinMode(analogPin, INPUT);
  pinMode(digitalPin, INPUT);

  Serial.println("ESP32 Watering System Ready (relay OFF=LOW).");
}

void loop() {

  // ESP32 ADC range: 0–4095
  int rawValue = analogRead(analogPin);
  int moisturePercent = map(rawValue, 4095, 0, 0, 100);

  int soilDO = digitalRead(digitalPin);

  // DO = HIGH → DRY, LOW → WET
  bool isDry = (soilDO == HIGH);

  unsigned long now = millis();

  // Start pump ONLY when soil is dry
  if (!pumpRunning && isDry && (now - lastPump >= MIN_INTERVAL_MS)) {
    pumpRunning = true;
    pumpStart = now;
    digitalWrite(pumpPin, PUMP_ON);
    Serial.println("Pump ON (soil dry)");
  }

  // Stop pump after set duration
  if (pumpRunning && (now - pumpStart >= PUMP_DURATION_MS)) {
    pumpRunning = false;
    lastPump = now;
    digitalWrite(pumpPin, PUMP_OFF);
    Serial.println("Pump OFF (duration complete)");
  }

  // Status output
  Serial.println("------ STATUS ------");
  Serial.print("Raw Value: "); Serial.println(rawValue);
  Serial.print("Moisture %: "); Serial.print(moisturePercent); Serial.println("%");
  Serial.print("DO pin: "); Serial.println(isDry ? "HIGH (DRY)" : "LOW (WET)");
  Serial.print("Pump: "); Serial.println(pumpRunning ? "RUNNING" : "OFF");
  Serial.println("--------------------");

  delay(1000);
}

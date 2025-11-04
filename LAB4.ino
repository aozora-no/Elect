#include <WiFi.h>

const char* ssid = "Reconnecting....";
const char* password = "https://Nuqui ";

WiFiServer server(80);
String header;

int ledPins[] = {13, 12, 14, 27, 26, 25, 33, 32};
int ledCount = 8;

String mode = "OFF";
unsigned long previousMillis = 0;
bool blinkState = false;
int runningIndex = 0;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < ledCount; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New User.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        // If we've reached the end of the HTTP request header (blank line)
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Decide response based on requested path
            if (header.indexOf("GET /running") >= 0) {
              mode = "RUNNING";
            } else if (header.indexOf("GET /high") >= 0) {
              mode = "HIGHLOW";
            } else if (header.indexOf("GET /evenodd") >= 0) {
              mode = "EVENODD";
            } else if (header.indexOf("GET /off") >= 0) {
              mode = "OFF";
              allLow();
            } else if (header.indexOf("GET /status") >= 0) {
              // Simple status endpoint used by the front-end JS
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println(mode);
              break;
            }


            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=UTF-8");
            client.println("Connection: close");
            client.println();

            
            client.println("<!DOCTYPE html><html lang='en'>");
            client.println("<head>");
            client.println("<meta charset='utf-8'>");
            client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0'>");
            client.println("<title>ESP32 LED Control</title>");
            client.println("<style>");
            client.println(":root{--bg1:#0f1724; --bg2:#0b3142; --card:#0f1724aa; --accent:#00d4a6; --muted:#9fb3bd; --danger:#ff6b6b}");
            client.println("html,body{height:100%;margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,'Helvetica Neue',Arial;color:#e6f7f5;background:linear-gradient(180deg,var(--bg1) 0%, #0a2230 100%);-webkit-font-smoothing:antialiased;}");
            client.println(".wrap{min-height:100%;display:flex;flex-direction:column;align-items:center;padding:20px;box-sizing:border-box}");
            client.println(".card{width:100%;max-width:420px;background:rgba(255,255,255,0.03);border-radius:14px;padding:18px;margin-top:28px;box-shadow:0 8px 30px rgba(2,6,23,0.6);backdrop-filter: blur(6px)}");
            client.println("h1{font-size:20px;margin:0 0 8px 0;color:#fff;text-align:center}");
            client.println(".subtitle{font-size:13px;color:var(--muted);text-align:center;margin-bottom:16px}");
            client.println(".controls{display:grid;grid-template-columns:1fr 1fr;gap:12px}");
            client.println("button{padding:12px 10px;border-radius:10px;border:0;font-weight:600;color:#062726;cursor:pointer;box-shadow:0 6px 16px rgba(2,6,23,0.4);transition:transform .08s ease, box-shadow .08s ease}");
            client.println(".btn-run{background:linear-gradient(180deg,#00d4a6,#00b089);grid-column:span 2;color:#04221d}");
            client.println(".btn-high{background:linear-gradient(180deg,#3ec2ff,#2b91d6)}");
            client.println(".btn-evenodd{background:linear-gradient(180deg,#b58eff,#7d59d8);color:#fff}");
            client.println(".btn-off{background:linear-gradient(180deg,#ff8b8b,#ff6b6b);color:#2b0606}");
            client.println("button:active{transform:translateY(1px);box-shadow:0 4px 10px rgba(2,6,23,0.45)}");
            client.println(".statusRow{display:flex;justify-content:space-between;align-items:center;margin-top:14px;padding:10px;border-radius:10px;background:linear-gradient(180deg, rgba(255,255,255,0.02), rgba(255,255,255,0.01));}");
            client.println(".statusLabel{font-size:13px;color:var(--muted)}");
            client.println(".statusValue{font-weight:700;padding:6px 10px;border-radius:999px;background:rgba(255,255,255,0.03);color:#fff}");
            client.println(".ip{font-size:12px;color:var(--muted);text-align:center;margin-top:12px}");
            client.println(".footer{font-size:11px;color:rgba(255,255,255,0.5);text-align:center;margin-top:10px}");
            client.println("@media (max-width:420px){.controls{grid-template-columns:1fr;}}");
            client.println("</style>");
            client.println("</head>");
            client.println("<body>");
            client.println("<div class='wrap'>");
            client.println("<div class='card'>");
            client.println("<h1>ESP32 LED Controller</h1>");
            

            client.println("<div class='controls'>");
            client.println("<button class='btn-run' onclick=\"post('/running')\">Running Light</button>");
            client.println("<button class='btn-high' onclick=\"post('/high')\">All High / Low</button>");
            client.println("<button class='btn-evenodd' onclick=\"post('/evenodd')\">Even / Odd Blink</button>");
            client.println("<button class='btn-off' onclick=\"post('/off')\">Turn Off</button>");
            client.println("</div>");
            client.println("<div class='statusRow'>");
            client.println("<div class='statusLabel'>Current Mode</div>");
            client.println("<div id='mode' class='statusValue'>--</div>");
            client.println("</div>");
            client.println("<div class='ip'>IP: " + WiFi.localIP().toString() + "</div>");
            client.println("<div class='footer'>Created by GaloNuqui</div>");
            client.println("</div>"); 
            client.println("</div>"); 

            
            client.println("<script>");
            client.println("function post(path){");
            client.println("  fetch(path).then(()=> setTimeout(refreshStatus,200));");
            client.println("}");
            client.println("function refreshStatus(){");
            client.println("  fetch('/status').then(r=>r.text()).then(t=>{");
            client.println("    document.getElementById('mode').textContent = t || '--';");
            client.println("  }).catch(()=>{document.getElementById('mode').textContent = 'OFF';});");
            client.println("}");
            client.println("// Poll mode every 1s so the UI stays in sync with changes");
            client.println("setInterval(refreshStatus,1000);");
            client.println("refreshStatus();");
            client.println("</script>");

            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("User disconnected.");
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 250) {
    previousMillis = currentMillis;
    blinkState = !blinkState;

    if (mode == "RUNNING") {
      allLow();
      digitalWrite(ledPins[runningIndex], HIGH);
      runningIndex = runningIndex + 1;
      if (runningIndex >= ledCount) {
        runningIndex = 0;
      }
    }

    else if (mode == "HIGHLOW") {
      for (int i = 0; i < ledCount; i++) {
        if (blinkState == true) {
          digitalWrite(ledPins[i], HIGH);
        } else {
          digitalWrite(ledPins[i], LOW);
        }
      }
    }

    else if (mode == "EVENODD") {
      for (int i = 0; i < ledCount; i++) {
        bool isEven = (i % 2 == 0);

        if (blinkState == true) {
          if (isEven == true) {
            digitalWrite(ledPins[i], HIGH);
          } else {
            digitalWrite(ledPins[i], LOW);
          }
        } else {
          if (isEven == true) {
            digitalWrite(ledPins[i], LOW);
          } else {
            digitalWrite(ledPins[i], HIGH);
          }
        }
      }
    }

    else if (mode == "OFF") {
      allLow();
    }
  }
}

void allLow() {
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}
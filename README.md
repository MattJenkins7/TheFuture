"Your real friends are easy to identify. 
 They don't ask for your wi-fi, they already have it,
 because trust isn't saying "Hey, what's the password?",
 trust is, your phone connecting automatically, and, if
 they still ask for your password, that's not a friend,
 that's a guest. *Laugh track*" - Miami Tony Stark

# ESP32 + Flask Live Dashboard

This setup gives you:

- Flask server endpoint for ESP32 sensor uploads
- In-memory latest readings storage
- Current LED pattern storage
- Web dashboard with live AJAX updates (no refresh)
- Pattern controls from web UI

## 1) Install and run Flask server

From this folder in PowerShell:

```powershell
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python app.py
```

Open: http://127.0.0.1:5000

## 2) ESP32 -> Flask payload format

Send POST requests to:

`http://<PC_IP>:5000/api/sensor-data`

JSON body example:

```json
{
  "temperature": 24.6,
  "humidity": 57.2,
  "light": 312,
  "pattern": "BLINK"
}
```

Use your computer's local IP for `<PC_IP>` (for example `192.168.1.20`) and ensure ESP32 is on the same Wi-Fi network.

## 3) Example ESP32 code snippet (Arduino/PlatformIO)

```cpp
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";
const char* serverUrl = "http://192.168.1.20:5000/api/sensor-data";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    float temperature = 24.6;
    float humidity = 57.2;
    int light = 312;
    String pattern = "BLINK";

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"temperature\":" + String(temperature, 1) + ",";
    json += "\"humidity\":" + String(humidity, 1) + ",";
    json += "\"light\":" + String(light) + ",";
    json += "\"pattern\":\"" + pattern + "\"";
    json += "}";

    int httpCode = http.POST(json);
    String response = http.getString();

    Serial.printf("POST code: %d\n", httpCode);
    Serial.println(response);

    http.end();
  }

  delay(2000);
}
```

## 4) Changing LED pattern from web page

The dashboard buttons call:

- `POST /api/pattern` with body `{ "pattern": "WAVE" }`

To make ESP32 follow this selected pattern, have ESP32 poll:

- `GET /api/status`

Then read `pattern` from the JSON response and update your LED logic.

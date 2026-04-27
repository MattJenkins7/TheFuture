#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char *WIFI_SSID = "AndroidAP33D3";
const char *WIFI_PASS = "password";

// Flask server
const char *SERVER_URL = "http://10.82.46.156:5000/api/sensor-data";
const char *STATUS_URL = "http://10.82.46.156:5000/api/status";

// LED pins
const int LED_1 = 6;
const int LED_2 = 9;
const int LED_3 = 10;
const int LED_4 = 11;
const int LED_5 = 12;

const int LED_PINS[5] = {LED_1, LED_2, LED_3, LED_4, LED_5};

enum PatternMode
{
    PATTERN_OFF,
    PATTERN_BLINK,
    PATTERN_CHASE,
    PATTERN_ALTERNATE,
    PATTERN_FLICKER,
    PATTERN_TEMPERATURE,
    PATTERN_MANUAL
};

PatternMode currentPatternMode = PATTERN_OFF;
String currentPattern = "OFF";
bool ledStates[5] = {false, false, false, false, false};
unsigned long lastPatternPollMs = 0;
const unsigned long patternPollIntervalMs = 300;
unsigned long lastPatternStepMs = 0;
int chaseStep = 0;
bool blinkOn = false;
int alternateIndex = 0;
const unsigned long speed = 500;

const int sensorPin = 5;

const float referenceVoltage = 3.3;
#define ADC_MAX 4095.0

// Temperature storage
float currentTemperature = 0.0;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=== TMP36 Temperature Sensor Test ===");

    analogReadResolution(12);
    analogSetPinAttenuation(sensorPin, ADC_11db);

    for (int i = 0; i < 5; i++)
    {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(300);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());

    randomSeed(analogRead(sensorPin));
}

static void setLeds(bool led1On, bool led2On, bool led3On, bool led4On, bool led5On)
{
    digitalWrite(LED_PINS[0], led1On ? HIGH : LOW);
    digitalWrite(LED_PINS[1], led2On ? HIGH : LOW);
    digitalWrite(LED_PINS[2], led3On ? HIGH : LOW);
    digitalWrite(LED_PINS[3], led4On ? HIGH : LOW);
    digitalWrite(LED_PINS[4], led5On ? HIGH : LOW);
}

static PatternMode parsePattern(const String &pattern)
{
    if (pattern == "BLINK")
        return PATTERN_BLINK;
    if (pattern == "WAVE")
        return PATTERN_CHASE;
    if (pattern == "RAINBOW")
        return PATTERN_ALTERNATE;
    if (pattern == "FLICKER")
        return PATTERN_FLICKER;
    if (pattern == "TEMPERATURE_RESPONSIVE")
        return PATTERN_TEMPERATURE;
    if (pattern == "MANUAL")
        return PATTERN_MANUAL;
    return PATTERN_OFF;
}

static void applyPattern(const String &pattern)
{
    currentPattern = pattern;
    currentPatternMode = parsePattern(pattern);
    lastPatternStepMs = 0;
    chaseStep = 0;
    blinkOn = false;
    alternateIndex = 0;

    if (currentPatternMode != PATTERN_MANUAL)
    {
        setLeds(false, false, false, false, false);
    }
}

static int getTemperatureAsBinary(float temp)
{
    int tempInt = (int)temp;
    if (tempInt < 0)
        tempInt = 0;
    if (tempInt > 31)
        tempInt = 31;
    return tempInt;
}

static void pollPattern()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    http.begin(STATUS_URL);
    int code = http.GET();
    if (code == 200)
    {
        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        if (!err)
        {
            const char *pattern = doc["pattern"] | "OFF";
            if (currentPattern != pattern)
            {
                applyPattern(pattern);
            }

            if (currentPattern == "MANUAL" && doc.containsKey("led_states"))
            {
                JsonArray states = doc["led_states"];
                if (states.size() == 5)
                {
                    for (int i = 0; i < 5; i++)
                    {
                        ledStates[i] = states[i].as<bool>();
                    }
                }
            }
        }
    }
    http.end();
}

static void updatePattern(unsigned long now)
{
    if (currentPatternMode == PATTERN_OFF)
    {
        return;
    }

    if (currentPatternMode == PATTERN_BLINK)
    {
        if (now - lastPatternStepMs >= speed)
        {
            lastPatternStepMs = now;
            blinkOn = !blinkOn;
            setLeds(blinkOn, blinkOn, blinkOn, blinkOn, blinkOn);
        }
        return;
    }

    if (currentPatternMode == PATTERN_CHASE)
    {
        if (now - lastPatternStepMs >= speed)
        {
            lastPatternStepMs = now;
            if (chaseStep == 0)
            {
                setLeds(true, false, false, false, false);
            }
            else if (chaseStep == 1)
            {
                setLeds(false, true, false, false, false);
            }
            else if (chaseStep == 2)
            {
                setLeds(false, false, true, false, false);
            }
            else if (chaseStep == 3)
            {
                setLeds(false, false, false, true, false);
            }
            else
            {
                setLeds(false, false, false, false, true);
            }
            chaseStep = (chaseStep + 1) % 5;
        }
        return;
    }

    if (currentPatternMode == PATTERN_ALTERNATE)
    {
        if (now - lastPatternStepMs >= speed)
        {
            lastPatternStepMs = now;
            if (alternateIndex < 5)
            {
                setLeds(
                    alternateIndex >= 0,
                    alternateIndex >= 1,
                    alternateIndex >= 2,
                    alternateIndex >= 3,
                    alternateIndex >= 4);
            }
            else
            {
                int downIndex = 9 - alternateIndex;
                setLeds(
                    downIndex >= 0,
                    downIndex >= 1,
                    downIndex >= 2,
                    downIndex >= 3,
                    downIndex >= 4);
            }
            alternateIndex = (alternateIndex + 1) % 10;
        }
        return;
    }

    if (currentPatternMode == PATTERN_FLICKER)
    {
        if (now - lastPatternStepMs >= speed / 2)
        {
            lastPatternStepMs = now;
            setLeds(
                random(0, 2),
                random(0, 2),
                random(0, 2),
                random(0, 2),
                random(0, 2));
        }
        return;
    }

    if (currentPatternMode == PATTERN_TEMPERATURE)
    {
        int tempInt = getTemperatureAsBinary(currentTemperature);
        setLeds(
            (tempInt >> 0) & 1,
            (tempInt >> 1) & 1,
            (tempInt >> 2) & 1,
            (tempInt >> 3) & 1,
            (tempInt >> 4) & 1);
        return;
    }

    if (currentPatternMode == PATTERN_MANUAL)
    {
        setLeds(ledStates[0], ledStates[1], ledStates[2], ledStates[3], ledStates[4]);
        return;
    }
}

void loop()
{
    int adcValue = analogRead(sensorPin);

    float voltage = adcValue * referenceVoltage / ADC_MAX;

    float temperatureC = (voltage - 0.5) * 100.0;
    currentTemperature = temperatureC;

    Serial.print("ADC: ");
    Serial.print(adcValue);
    Serial.print("  Voltage: ");
    Serial.print(voltage, 3);
    Serial.print("V  Temperature: ");
    Serial.print(temperatureC, 2);
    Serial.println("C");

    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(SERVER_URL);
        http.addHeader("Content-Type", "application/json");

        String payload = "{";
        payload += "\"temperature\":" + String(temperatureC, 2);
        payload += "}";

        int code = http.POST(payload);
        Serial.print("POST code: ");
        Serial.println(code);
        if (code > 0)
        {
            String resp = http.getString();
            Serial.println(resp);
        }
        http.end();
    }
    else
    {
        Serial.println("WiFi disconnected. Reconnecting...");
        WiFi.reconnect();
    }

    unsigned long now = millis();
    if (now - lastPatternPollMs >= patternPollIntervalMs)
    {
        lastPatternPollMs = now;
        pollPattern();
    }

    updatePattern(now);

    delay(500);
}
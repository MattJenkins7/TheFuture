#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char *WIFI_SSID = "AndroidAP33D3";
const char *WIFI_PASS = "password";

// Flask server endpoints
const char *SENSOR_DATA_ENDPOINT = "http://10.82.46.156:5000/api/sensor-data";
const char *LED_PATTERN_ENDPOINT = "http://10.82.46.156:5000/api/status";

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
    PATTERN_WAVE,
    PATTERN_FLICKER,
    PATTERN_TEMPERATURE,
    PATTERN_MANUAL
};

PatternMode currentPatternMode = PATTERN_OFF;
String currentPattern = "OFF";
bool ledStates[5] = {false, false, false, false, false};
unsigned long lastStatusCheckTime = 0;
const unsigned long statusCheckInterval = 300; // ms between pattern polling
unsigned long lastAnimationTime = 0;
int wavePosition = 0;            // current position in wave/chase pattern
bool isBlinkOn = false;          // tracks blink on/off state
int glowIndex = 0;               // position in glow/alternate animation
const unsigned long speed = 500; // ms between animation steps

const int TEMP_SENSOR_PIN = 5;

const float referenceVoltage = 3.3;
#define ADC_MAX 4095.0

// Current temperature reading in Celsius
float currentTemperature = 0.0;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=== TMP36 Temperature Sensor ===");

    analogReadResolution(12);
    analogSetPinAttenuation(TEMP_SENSOR_PIN, ADC_11db);

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

    randomSeed(analogRead(TEMP_SENSOR_PIN));
}

static void setLeds(bool led1, bool led2, bool led3, bool led4, bool led5)
{
    digitalWrite(LED_PINS[0], led1 ? HIGH : LOW);
    digitalWrite(LED_PINS[1], led2 ? HIGH : LOW);
    digitalWrite(LED_PINS[2], led3 ? HIGH : LOW);
    digitalWrite(LED_PINS[3], led4 ? HIGH : LOW);
    digitalWrite(LED_PINS[4], led5 ? HIGH : LOW);
}

static PatternMode parsePattern(const String &pattern)
{
    if (pattern == "BLINK")
        return PATTERN_BLINK;
    if (pattern == "CHASE")
        return PATTERN_CHASE;
    if (pattern == "WAVE")
        return PATTERN_WAVE;
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
    lastAnimationTime = 0;
    wavePosition = 0;
    isBlinkOn = false;
    glowIndex = 0;

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
    http.begin(LED_PATTERN_ENDPOINT);
    int httpStatus = http.GET();
    if (httpStatus == 200)
    {
        StaticJsonDocument<1024> jsonResponse;
        DeserializationError parseError = deserializeJson(jsonResponse, http.getStream());
        const char *pattern = jsonResponse["pattern"] | "OFF";
        if (currentPattern != pattern)
        {
            applyPattern(pattern);
        }

        if (currentPattern == "MANUAL" && jsonResponse.containsKey("led_states"))
        {
            JsonArray states = jsonResponse["led_states"];
            if (states.size() == 5)
            {
                for (int i = 0; i < 5; i++)
                {
                    ledStates[i] = states[i].as<bool>();
                }
            }
        }
    }
    http.end();
}

static void updatePattern(unsigned long currentTime)
{
    // Pattern off - do nothing
    if (currentPatternMode == PATTERN_OFF)
    {
        return;
    }

    // Pattern Blink - toggle all LEDs on/off
    if (currentPatternMode == PATTERN_BLINK)
    {
        if (currentTime - lastAnimationTime >= speed)
        {
            lastAnimationTime = currentTime;
            isBlinkOn = !isBlinkOn;
            setLeds(isBlinkOn, isBlinkOn, isBlinkOn, isBlinkOn, isBlinkOn);
        }
        return;
    }

    // Pattern Chase - light up one LED at a time in a chase pattern
    if (currentPatternMode == PATTERN_CHASE)
    {
        if (currentTime - lastAnimationTime >= speed)
        {
            lastAnimationTime = currentTime;
            for (int i = 0; i < 5; i++)
            {
                digitalWrite(LED_PINS[i], (i == wavePosition) ? HIGH : LOW);
            }
            wavePosition = (wavePosition + 1) % 5;
        }
        return;
    }

    // Pattern Wave - Light up LEDs in a wave pattern
    if (currentPatternMode == PATTERN_WAVE)
    {
        if (currentTime - lastAnimationTime >= speed)
        {
            lastAnimationTime = currentTime;
            int activeLeds = (glowIndex < 5) ? glowIndex : (9 - glowIndex);
            for (int i = 0; i < 5; i++)
            {
                digitalWrite(LED_PINS[i], (i < activeLeds) ? HIGH : LOW);
            }
            glowIndex = (glowIndex + 1) % 10;
        }
        return;
    }

    if (currentPatternMode == PATTERN_FLICKER)
    {
        if (currentTime - lastAnimationTime >= speed / 2)
        {
            lastAnimationTime = currentTime;
            for (int i = 0; i < 5; i++)
            {
                digitalWrite(LED_PINS[i], random(0, 2) ? HIGH : LOW);
            }
        }
        return;
    }

    if (currentPatternMode == PATTERN_TEMPERATURE)
    {
        int tempBits = getTemperatureAsBinary(currentTemperature);
        for (int i = 0; i < 5; i++)
        {
            digitalWrite(LED_PINS[i], ((tempBits >> i) & 1) ? HIGH : LOW);
        }
        return;
    }

    if (currentPatternMode == PATTERN_MANUAL)
    {
        for (int i = 0; i < 5; i++)
        {
            digitalWrite(LED_PINS[i], ledStates[i] ? HIGH : LOW);
        }
        return;
    }
}

void loop()
{
    int adcValue = analogRead(TEMP_SENSOR_PIN);

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
        http.begin(SENSOR_DATA_ENDPOINT);
        http.addHeader("Content-Type", "application/json");

        String payload = "{";
        payload += "\"temperature\":" + String(temperatureC, 2);
        payload += "}";

        int httpStatus = http.POST(payload);
        Serial.print("POST status: ");
        Serial.println(httpStatus);
        if (httpStatus > 0)
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

    unsigned long currentTime = millis();
    if (currentTime - lastStatusCheckTime >= statusCheckInterval)
    {
        lastStatusCheckTime = currentTime;
        pollPattern();
    }

    updatePattern(currentTime);

    delay(3000);
}
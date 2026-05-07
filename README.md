## Overview

This project implements an ESP32-based IoT system that reads temperature data via a TMP36 sensor and controls 5 LEDs with various animated patterns. The system communicates with a Python Flask server for pattern control and sensor data logging.

**Features:**

- Real-time temperature sensing (TMP36 sensor)
- 5 LED control with multiple animation patterns
- Web-based dashboard for pattern selection and manual LED control
- Live temperature graph visualization
- WiFi connectivity with automatic reconnection
- Temperature-responsive LED patterns
- Manual LED control via web interface

## Hardware

- **Microcontroller:** ESP32
- **Sensor:** TMP36 Temperature Sensor (GPIO 5)
- **LEDs:** 5 individual LEDs on pins 6, 9, 10, 11, 12
- **WiFi:** Built-in ESP32 WiFi

## Available LED Patterns

- **OFF** - All LEDs off
- **BLINK** - All LEDs toggle on/off together
- **CHASE** - Single LED moves across the array sequentially
- **WAVE** - LEDs pulse on gradually (0→5→0)
- **FLICKER** - Random rapid on/off flicker effect
- **TEMPERATURE_RESPONSIVE** - LEDs display temperature as binary representation
- **MANUAL** - Control individual LED states from web interface

## Installation and Setup

### 1) Install and run Flask server

From this folder in PowerShell:

```powershell
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python app.py
```

Open: http://127.0.0.1:5000

### 2) ESP32 Configuration

Update the WiFi credentials and server IP in `src/main.cpp`:

```cpp
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char *SENSOR_DATA_ENDPOINT = "http://YOUR_PC_IP:5000/api/sensor-data";
const char *LED_PATTERN_ENDPOINT = "http://YOUR_PC_IP:5000/api/status";
```

### 3) Build and Upload

Using PlatformIO:

```bash
platformio run --target upload --upload-port COM3
```

## API Endpoints

### POST /api/sensor-data

ESP32 sends temperature data every 3 seconds:

```json
{
  "temperature": 18.6
}
```

### GET /api/status

Returns current pattern and LED states:

```json
{
  "temperature": 18.6,
  "pattern": "WAVE",
  "led_states": [false, true, false, true, false],
  "updated_at": "2026-05-07T12:00:00"
}
```

### POST /api/pattern

Set LED pattern from web interface:

```json
{
  "pattern": "CHASE"
}
```

Valid patterns: OFF, BLINK, CHASE, WAVE, FLICKER, TEMPERATURE_RESPONSIVE, MANUAL

### POST /api/manual-led

Control individual LED states:

```json
{
  "states": [true, false, true, false, false]
}
```

### GET /api/temperature-history

Retrieve last 60 temperature readings with timestamps

## Web Interface

Access the dashboard at `http://YOUR_PC_IP:5000`:

- **Temperature Display** - Shows live temperature reading
- **Pattern Buttons** - Select LED animation patterns
- **Manual LED Control** - Toggle individual LEDs when MANUAL pattern is selected
- **Temperature Mode** - Activate temperature-responsive LED display
- **Temperature Graph** - Real-time chart of temperature history

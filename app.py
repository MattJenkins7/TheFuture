from flask import Flask, jsonify, render_template, request
from threading import Lock
from datetime import datetime, timezone
from collections import deque

app = Flask(__name__)
state_lock = Lock()

# Valid LED patterns
VALID_PATTERNS = ["OFF", "BLINK", "WAVE", "RAINBOW", "FLICKER", "TEMPERATURE_RESPONSIVE"]

# In-memory state for latest readings and LED pattern.
latest_state = {
    "temperature": None,
    "humidity": None,
    "light": None,
    "pattern": "OFF",
    "speed": 300,
    "updated_at": None,
}

# Temperature history (store last 60 readings for ~5 minutes at 5sec intervals)
temperature_history = deque(maxlen=60)

# Deviation threshold for anomaly detection (°C)
TEMP_ANOMALY_THRESHOLD = 2.0


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/api/status")
def get_status():
    with state_lock:
        return jsonify(latest_state)


@app.post("/api/sensor-data")
def post_sensor_data():
    payload = request.get_json(silent=True) or {}

    with state_lock:
        if "temperature" in payload:
            new_temp = payload["temperature"]
            
            # Check if this reading is an anomaly by comparing to last recorded reading
            if temperature_history:
                last_recorded = temperature_history[-1]["temp"]
                # Only add temperature if it's consistent (within threshold) or represents a gradual change
                if abs(new_temp - last_recorded) <= TEMP_ANOMALY_THRESHOLD:
                    temperature_history.append({
                        "temp": new_temp,
                        "time": datetime.now(timezone.utc).isoformat()
                    })
                # If deviation is too large, skip this reading to filter glitches
            else:
                # First reading, always accept
                temperature_history.append({
                    "temp": new_temp,
                    "time": datetime.now(timezone.utc).isoformat()
                })
            
            latest_state["temperature"] = new_temp
        if "light" in payload:
            latest_state["light"] = payload["light"]
        if "pattern" in payload:
            latest_state["pattern"] = str(payload["pattern"])

        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "state": latest_state})


@app.get("/api/patterns")
def get_patterns():
    return jsonify({"patterns": VALID_PATTERNS})


@app.post("/api/pattern")
def set_pattern():
    payload = request.get_json(silent=True) or {}
    pattern = payload.get("pattern", "").upper()

    if not pattern or pattern not in VALID_PATTERNS:
        return jsonify({
            "ok": False, 
            "error": f"Invalid pattern. Valid patterns: {', '.join(VALID_PATTERNS)}"
        }), 400

    with state_lock:
        latest_state["pattern"] = pattern
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "pattern": latest_state["pattern"]})


@app.post("/api/speed")
def set_speed():
    payload = request.get_json(silent=True) or {}
    speed = payload.get("speed")

    if speed is None or not isinstance(speed, int) or speed < 500 or speed > 3000:
        return jsonify({
            "ok": False,
            "error": "Speed must be an integer between 500 and 3000 ms"
        }), 400

    with state_lock:
        latest_state["speed"] = speed
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "speed": latest_state["speed"]})


@app.get("/api/temperature-history")
def get_temperature_history():
    with state_lock:
        return jsonify({"history": list(temperature_history)})


@app.post("/api/temperature-responsive")
def set_temperature_responsive():
    payload = request.get_json(silent=True) or {}
    enabled = payload.get("enabled", False)

    with state_lock:
        if enabled:
            latest_state["pattern"] = "TEMPERATURE_RESPONSIVE"
        else:
            latest_state["pattern"] = "OFF"
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "pattern": latest_state["pattern"]})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)

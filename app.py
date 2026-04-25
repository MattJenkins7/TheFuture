from flask import Flask, jsonify, render_template, request
from threading import Lock
from datetime import datetime, timezone
from collections import deque

app = Flask(__name__)
state_lock = Lock()

# Valid LED patterns
VALID_PATTERNS = ["OFF", "BLINK", "WAVE", "RAINBOW", "FLICKER", "TEMPERATURE_RESPONSIVE", "MANUAL"]

# In-memory state for latest readings and LED pattern.
latest_state = {
    "temperature": None,
    "humidity": None,
    "light": None,
    "pattern": "OFF",
    "speed": 500,
    "led_states": [True, False, False, False, False],
    "updated_at": None,
}

# Temperature history (store last 60 readings for ~5 minutes at 5sec intervals)
temperature_history = deque(maxlen=60)

# Deviation threshold for anomaly detection (°C)
TEMP_ANOMALY_THRESHOLD = 2


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
            
            # Only add to history if change is reasonable (ignore spikes > 5°C)
            should_record = False
            if temperature_history:
                last_recorded = temperature_history[-1]["temp"]
                # Accept if change is <= 5°C
                if abs(new_temp - last_recorded) <= 5:
                    should_record = True
            else:
                # First reading, always accept
                should_record = True
            
            if should_record:
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


@app.post("/api/manual-led")
def set_manual_led():
    payload = request.get_json(silent=True) or {}
    states = payload.get("states")

    if not states or not isinstance(states, list) or len(states) != 5:
        return jsonify({
            "ok": False,
            "error": "States must be a list of 5 boolean values"
        }), 400

    for state in states:
        if not isinstance(state, bool):
            return jsonify({
                "ok": False,
                "error": "Each state must be a boolean (true/false)"
            }), 400

    with state_lock:
        latest_state["led_states"] = states
        latest_state["pattern"] = "MANUAL"
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "states": latest_state["led_states"], "pattern": "MANUAL"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)

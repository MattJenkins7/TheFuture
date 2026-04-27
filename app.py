from flask import Flask, jsonify, render_template, request
from threading import Lock
from datetime import datetime, timezone
from collections import deque

app = Flask(__name__)
state_lock = Lock()

VALID_PATTERNS = ["OFF", "BLINK", "WAVE", "RAINBOW", "FLICKER", "TEMPERATURE_RESPONSIVE", "MANUAL"]

latest_state = {
    "temperature": None,
    "pattern": "OFF",
    "led_states": [True, False, False, False, False],
    "updated_at": None,
}

temperature_history = deque(maxlen=60)


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
            if not temperature_history or abs(new_temp - temperature_history[-1]["temp"]) <= 5:
                temperature_history.append({
                    "temp": new_temp,
                    "time": datetime.now(timezone.utc).isoformat()
                })
            
            latest_state["temperature"] = new_temp

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
        latest_state["pattern"] = "TEMPERATURE_RESPONSIVE" if enabled else "OFF"
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "pattern": latest_state["pattern"]})


@app.post("/api/manual-led")
def set_manual_led():
    payload = request.get_json(silent=True) or {}
    states = payload.get("states")

    if not isinstance(states, list) or len(states) != 5 or not all(isinstance(s, bool) for s in states):
        return jsonify({
            "ok": False,
            "error": "States must be a list of 5 boolean values"
        }), 400

    with state_lock:
        latest_state["led_states"] = states
        latest_state["pattern"] = "MANUAL"
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "states": latest_state["led_states"], "pattern": "MANUAL"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)

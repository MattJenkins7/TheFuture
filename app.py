from flask import Flask, jsonify, render_template, request
from threading import Lock
from datetime import datetime, timezone

app = Flask(__name__)
state_lock = Lock()

# In-memory state for latest readings and LED pattern.
latest_state = {
    "temperature": None,
    "humidity": None,
    "light": None,
    "pattern": "OFF",
    "updated_at": None,
}


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
            latest_state["temperature"] = payload["temperature"]
        if "light" in payload:
            latest_state["light"] = payload["light"]
        if "pattern" in payload:
            latest_state["pattern"] = str(payload["pattern"])

        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "state": latest_state})


@app.post("/api/pattern")
def set_pattern():
    payload = request.get_json(silent=True) or {}
    pattern = payload.get("pattern")

    if not pattern:
        return jsonify({"ok": False, "error": "Missing 'pattern'"}), 400

    with state_lock:
        latest_state["pattern"] = str(pattern)
        latest_state["updated_at"] = datetime.now(timezone.utc).isoformat()

    return jsonify({"ok": True, "pattern": latest_state["pattern"]})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)

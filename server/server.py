from datetime import datetime
 
from flask import Flask, jsonify, request
 
app = Flask(__name__)
 
# Soil calibration: raw ESP32 readings (0-4095) with the sensor on 3.3V.
# "default" is used for any plant that doesn't have its own entry.
SOIL_CALIBRATION = {
    "default": {"wet": 1470, "dry": 3450},
    # "plant-2": {"wet": 1500, "dry": 3400},
}
 
 
def fmt(value, unit=""):
    # Formats a reading, or shows '--' if the sensor sent null.
    return "--" if value is None else f"{value}{unit}"
 
 
def soil_percent(raw, plant):
    # Converts a raw reading to 0% (dry air) - 100% (in water).
    cal = SOIL_CALIBRATION.get(plant, SOIL_CALIBRATION["default"])
    percent = (cal["dry"] - raw) / (cal["dry"] - cal["wet"]) * 100
    return max(0, min(100, round(percent)))
 
 
@app.post("/data")
def receive_data():
    data = request.get_json(silent=True)
    if not data:
        return jsonify(error="expected JSON"), 400
 
    now = datetime.now().strftime("%H:%M:%S")
    device = data.get("device", request.remote_addr)
 
    if "soil_raw" in data:
        plant = data.get("plant", "unknown plant")
        raw = data.get("soil_raw")
        if raw is None:
            line = f"{plant}  Soil: --"
        else:
            line = f"{plant}  Soil: {soil_percent(raw, plant)}%  (raw {raw})"
    else:
        line = (f"Temp: {fmt(data.get('temp_f'), '°F')}  "
                f"Humidity: {fmt(data.get('humidity'), '%')}  "
                f"Pressure: {fmt(data.get('pressure'), ' hPa')}")
 
    print(f"[{now}] {device:<14} {line}", flush=True)
    return jsonify(status="ok")
 
 
if __name__ == "__main__":
    # 0.0.0.0 = accept connections from other devices on the network
    app.run(host="0.0.0.0", port=5000)

from datetime import datetime

from flask import Flask, jsonify, request

app = Flask(__name__)


def fmt(value, unit=""):
    # Formats a reading, or shows '--' if the sensor sent null.
    return "--" if value is None else f"{value}{unit}"


@app.post("/data")
def receive_data():
    data = request.get_json(silent=True)
    if not data:
        return jsonify(error="expected JSON"), 400

    now = datetime.now().strftime("%H:%M:%S")
    device = data.get("device", request.remote_addr)

    print(f"[{now}] {device:<14} "
          f"Temp: {fmt(data.get('temp_f'), '°F')}  "
          f"Humidity: {fmt(data.get('humidity'), '%')}  "
          f"Pressure: {fmt(data.get('pressure'), ' hPa')}",
          flush=True)
    return jsonify(status="ok")


if __name__ == "__main__":
    # 0.0.0.0 = accept connections from other devices on the network
    app.run(host="0.0.0.0", port=5000)

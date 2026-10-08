#!/usr/bin/env python3
"""Receives sensor readings from the ESP32 nodes, prints them and saves
them to a PostgreSQL database (which Grafana reads from).

Two kinds of message arrive at /data:
    environment node: {"device": "env-node", "ts": 1791226930, "temp_f": 72.4,
                       "humidity": 45.2, "pressure": 1002.3}
    plant node:       {"device": "plant-node-1", "ts": 1791226930,
                       "plant": "plant-1", "soil_raw": 1830}

"ts" is the time the reading was taken, in seconds since 1970 (UTC). The
nodes take readings exactly on the clock (e.g. every 10 seconds), so
readings from different nodes share the same timestamps.

Normally this runs in Docker (see docker-compose.yml), which sets the
DB_* settings below. If DB_HOST isn't set, it only prints readings.
"""

import os
import time
from datetime import datetime, timezone

from flask import Flask, jsonify, request

app = Flask(__name__)

# ---------- SETTINGS ----------
# Soil calibration: raw ESP32 readings (0-4095) with the sensor on 3.3V.
# "default" is used for any plant that doesn't have its own entry.
SOIL_CALIBRATION = {
    "default": {"wet": 1470, "dry": 3450},
    # "plant-2": {"wet": 1500, "dry": 3400},
}

# Database connection, filled in by docker-compose.yml
DB_HOST = os.environ.get("DB_HOST")
DB_SETTINGS = {
    "host": DB_HOST,
    "port": os.environ.get("DB_PORT", "5432"),
    "dbname": os.environ.get("DB_NAME", "plantmonitor"),
    "user": os.environ.get("DB_USER", "plant"),
    "password": os.environ.get("DB_PASSWORD", ""),
}


# ---------- DATABASE ----------
CREATE_TABLES = """
CREATE TABLE IF NOT EXISTS env_readings (
    ts        TIMESTAMPTZ NOT NULL,
    device    TEXT NOT NULL,
    temp_f    REAL,
    humidity  REAL,
    pressure  REAL,
    PRIMARY KEY (ts, device)
);

CREATE TABLE IF NOT EXISTS soil_readings (
    ts            TIMESTAMPTZ NOT NULL,
    device        TEXT NOT NULL,
    plant         TEXT NOT NULL,
    soil_raw      INTEGER,
    soil_percent  REAL,
    PRIMARY KEY (ts, device, plant)
);
"""


def db_connect():
    import psycopg  # only needed when a database is configured
    return psycopg.connect(**DB_SETTINGS)


def init_db():
    """Creates the tables. Waits for the database if it's still starting."""
    for attempt in range(30):
        try:
            with db_connect() as conn:
                conn.execute(CREATE_TABLES)
            print("Database ready", flush=True)
            return
        except Exception as e:
            print(f"Waiting for database ({e.__class__.__name__})...", flush=True)
            time.sleep(2)
    raise RuntimeError("Could not connect to the database")


def save_env(taken_at, device, data):
    with db_connect() as conn:
        conn.execute(
            "INSERT INTO env_readings (ts, device, temp_f, humidity, pressure) "
            "VALUES (%s, %s, %s, %s, %s) ON CONFLICT DO NOTHING",
            (taken_at, device, data.get("temp_f"), data.get("humidity"),
             data.get("pressure")),
        )


def save_soil(taken_at, device, plant, raw, percent):
    with db_connect() as conn:
        conn.execute(
            "INSERT INTO soil_readings (ts, device, plant, soil_raw, soil_percent) "
            "VALUES (%s, %s, %s, %s, %s) ON CONFLICT DO NOTHING",
            (taken_at, device, plant, raw, percent),
        )


# ---------- READINGS ----------
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

    # Use the time the reading was taken if the node sent one;
    # otherwise fall back to the time it arrived. Stored in UTC.
    ts = data.get("ts")
    taken_at = (datetime.fromtimestamp(ts, tz=timezone.utc) if ts
                else datetime.now(timezone.utc))
    device = data.get("device", request.remote_addr)

    if "soil_raw" in data:
        plant = data.get("plant", "unknown plant")
        raw = data.get("soil_raw")
        percent = None if raw is None else soil_percent(raw, plant)
        line = (f"{plant}  Soil: --" if raw is None
                else f"{plant}  Soil: {percent}%  (raw {raw})")
        if DB_HOST:
            save_soil(taken_at, device, plant, raw, percent)
    else:
        line = (f"Temp: {fmt(data.get('temp_f'), '°F')}  "
                f"Humidity: {fmt(data.get('humidity'), '%')}  "
                f"Pressure: {fmt(data.get('pressure'), ' hPa')}")
        if DB_HOST:
            save_env(taken_at, device, data)

    local_time = taken_at.astimezone().strftime("%H:%M:%S")
    print(f"[{local_time}] {device:<14} {line}", flush=True)
    return jsonify(status="ok")


@app.get("/health")
def health():
    return jsonify(status="ok")


if DB_HOST:
    init_db()
else:
    print("DB_HOST not set: printing readings only, not saving them", flush=True)

if __name__ == "__main__":
    # 0.0.0.0 = accept connections from other devices on the network
    app.run(host="0.0.0.0", port=5000)

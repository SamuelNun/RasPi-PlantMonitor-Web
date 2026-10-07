# Plant Monitor

A plant monitoring system built from ESP32 sensor nodes and a Raspberry Pi server. One ESP32 measures the room's temperature, humidity and pressure, another measures soil moisture at the plant, and both send their readings over Wi-Fi to the Pi.

This is a Python rewrite of an earlier Java Swing version that ran on a Raspberry Pi touchscreen.

## How it works

<img width="1250" height="511" alt="plantmonitordiagram" src="https://github.com/user-attachments/assets/c3997e5f-b9ba-43ac-9fb0-f1249e392fd9" />


Each node includes its name in every message, so the server can tell them apart:

```json
{"device": "env-node", "ts": 1791226800, "temp_f": 72.4, "humidity": 45.2, "pressure": 1002.3}
{"device": "plant-node-1", "ts": 1791226800, "plant": "plant-1", "soil_raw": 1830}
```

Readings are taken exactly on the clock, every 5 minutes (`12:00:00`, `12:05:00`, ...). Each ESP32 sets its clock over the internet with NTP and puts the time of the reading in `ts`, so readings from different nodes line up on the same timestamps for graphing. The constant variable `SEND_INTERVAL_S` changes the intervals.

The Pi runs a Flask server that receives each reading at `/data` and prints it to the terminal. Soil readings are converted from the raw sensor value to a moisture percentage. If the BME280 isn't detected, its fields are sent as `null`.

```
[14:05:00] env-node       Temp: 72.4°F  Humidity: 45.2%  Pressure: 1002.3 hPa
[14:05:00] plant-node-1   plant-1  Soil: 50%  (raw 2460)
```

A plant node can read up to six soil sensors (one per ADC1 pin), so one ESP32 can cover several plants that sit close together.

## Hardware

| Part                                         | Purpose                            |
| -------------------------------------------- | ---------------------------------- |
| Raspberry Pi 5 (Raspberry Pi OS Lite 64-bit) | Server                             |
| 2× ESP32                                     | Sensor nodes, send data over Wi-Fi |
| GY-BME280 (6-pin, 3.3V only)                 | Temperature, humidity, pressure    |
| Capacitive soil moisture sensor v1.2         | Soil moisture                      |

## Wiring

All sensors connect to the ESP32s. Nothing connects to the Pi's GPIO header.

**Environment node: BME280**

|BME280 pin|ESP32 pin|
|---|---|
|VCC|3V3|
|GND|GND|
|SCL|GPIO21|
|SDA|GPIO22|
|CSB|3V3 (selects I2C mode)|
|SDO|GND (sets I2C address to 0x76)|

**Plant node: soil moisture sensor**

|Sensor pin|ESP32 pin|
|---|---|
|VCC|3V3|
|GND|GND|
|AOUT|GPIO34|

GPIO34 is an ADC1 pin, which keeps working while Wi-Fi is on. ADC2 pins don't. For more plants, use the other ADC1 pins (32, 33, 35, 36, 39) and add a line per plant to `PLANTS` in `plant_node.ino`.

## Setup

### 1. Raspberry Pi server

```bash
cd ~/projects/plantmonitor
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python server/server.py
```

Give the Pi a DHCP reservation on the router so its IP address doesn't change. The ESP32s need a fixed address to send to.

### 2. ESP32 firmware

Install `arduino-cli`, ESP32 board support and the BME280 library:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit BME280 Library"
```

Each sketch needs its own `secrets.h` with your Wi-Fi details and the Pi's IP address. Create one from the template and copy it to the other sketch:

```bash
cp firmware/env_node/secrets.example.h firmware/env_node/secrets.h
nano firmware/env_node/secrets.h
cp firmware/env_node/secrets.h firmware/plant_node/secrets.h
```

Plug in one ESP32 at a time and upload the sketch for that board:

```bash
cd firmware

# Board with the BME280
arduino-cli compile --fqbn esp32:esp32:esp32 env_node
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 \
    --board-options UploadSpeed=115200 env_node

# Board with the soil sensor
arduino-cli compile --fqbn esp32:esp32:esp32 plant_node
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 \
    --board-options UploadSpeed=115200 plant_node
```

To watch an ESP32's output, use:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Once a board is flashed it only needs power, so it can run from any USB charger.

## Soil calibration

The soil sensor gives a raw reading from 0 to 4095. The server converts it to a percentage using two reference readings, set in `SOIL_CALIBRATION` at the top of `server.py`:

|Condition|Raw reading|Shown as|
|---|---|---|
|Sensor in dry air|3450|0%|
|Sensor in water|1470|100%|

These values were measured with the sensor powered from 3.3V. To recalibrate, take a reading in dry air and one in water and update the two numbers. Each plant can have its own entry.

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, SERVER_URL

// ---------- SETTINGS ----------
const char* DEVICE_NAME = "env-node";
const int SDA_PIN = 22;
const int SCL_PIN = 21;
const uint8_t BME_ADDR = 0x76;
const unsigned long SEND_INTERVAL_MS = 10000;

Adafruit_BME280 bme;
bool bmeOk = false;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed, will retry next loop");
  }
}

void sendJson(const String& json) {
  Serial.println(json);
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  Serial.printf("Sent to server, response: %d\n", code);  // 200 = OK
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  bmeOk = bme.begin(BME_ADDR);
  if (!bmeOk) Serial.println("BME280 not found, check wiring");

  connectWiFi();
}

void loop() {
  connectWiFi();

  // If the sensor wasn't found at startup, try again each loop
  if (!bmeOk) bmeOk = bme.begin(BME_ADDR);

  String json = "{\"device\":\"" + String(DEVICE_NAME) + "\",";
  if (bmeOk) {
    float tempF = bme.readTemperature() * 9.0 / 5.0 + 32.0;
    json += "\"temp_f\":" + String(tempF, 1) + ",";
    json += "\"humidity\":" + String(bme.readHumidity(), 1) + ",";
    json += "\"pressure\":" + String(bme.readPressure() / 100.0, 1) + "}";
  } else {
    json += "\"temp_f\":null,\"humidity\":null,\"pressure\":null}";
  }

  sendJson(json);
  delay(SEND_INTERVAL_MS);
}

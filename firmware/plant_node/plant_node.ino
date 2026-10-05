#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, SERVER_URL

// ---------- SETTINGS ----------
const char* DEVICE_NAME = "plant-node-1";
const unsigned long SEND_INTERVAL_MS = 10000;

// One line per soil sensor: {plant name, pin}. Add more lines for more plants.
struct Plant {
  const char* name;
  int pin;
};

const Plant PLANTS[] = {
  {"plant-1", 34},
  // {"plant-2", 35},
};
const int PLANT_COUNT = sizeof(PLANTS) / sizeof(PLANTS[0]);

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

// The ESP32's analog readings are noisy, so average several samples.
int readSoil(int pin) {
  long total = 0;
  for (int i = 0; i < 16; i++) {
    total += analogRead(pin);
    delay(5);
  }
  return total / 16;   // 0-4095
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogSetAttenuation(ADC_11db);  // lets the ADC read up to ~3.1V
  connectWiFi();
}

void loop() {
  connectWiFi();

  for (int i = 0; i < PLANT_COUNT; i++) {
    String json = "{\"device\":\"" + String(DEVICE_NAME) + "\",";
    json += "\"plant\":\"" + String(PLANTS[i].name) + "\",";
    json += "\"soil_raw\":" + String(readSoil(PLANTS[i].pin)) + "}";
    sendJson(json);
  }

  delay(SEND_INTERVAL_MS);
}

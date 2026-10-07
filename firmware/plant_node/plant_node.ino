#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <sys/time.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, SERVER_URL

// ---------- SETTINGS ----------
const char* DEVICE_NAME = "plant-node-1";
const int SEND_INTERVAL_S = 300;   

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

// ---------- CLOCK ----------
// The clock is "set" once it reports a date after 2023.
bool clockIsSet() {
  return time(nullptr) > 1700000000;
}

// Gets the current time from internet time servers (NTP).
void syncClock() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("Syncing clock");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // UTC
  unsigned long start = millis();
  while (!clockIsSet() && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(clockIsSet() ? " done" : " failed, will retry");
}

// Waits until the clock reaches the next multiple of SEND_INTERVAL_S and
// returns that time (seconds since 1970, UTC). Returns 0 if the clock
// isn't set, after a plain wait.
time_t waitForNextSlot() {
  if (!clockIsSet()) {
    delay(SEND_INTERVAL_S * 1000UL);
    return 0;
  }

  struct timeval now;
  gettimeofday(&now, nullptr);
  time_t slot = (now.tv_sec / SEND_INTERVAL_S + 1) * SEND_INTERVAL_S;
  long msLeft = (slot - now.tv_sec) * 1000L - now.tv_usec / 1000;
  if (msLeft > 0) delay(msLeft);
  return slot;
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
  syncClock();
}

void loop() {
  connectWiFi();
  if (!clockIsSet()) syncClock();

  time_t slot = waitForNextSlot();

  for (int i = 0; i < PLANT_COUNT; i++) {
    String json = "{\"device\":\"" + String(DEVICE_NAME) + "\",";
    if (slot != 0) json += "\"ts\":" + String((unsigned long)slot) + ",";
    json += "\"plant\":\"" + String(PLANTS[i].name) + "\",";
    json += "\"soil_raw\":" + String(readSoil(PLANTS[i].pin)) + "}";
    sendJson(json);
  }
}

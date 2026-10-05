#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, SERVER_URL

int count = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Sending to: ");
  Serial.println(SERVER_URL);
}

void loop() {
  count++;
  String json = "{\"device\":\"esp32-test\",\"message\":\"hello\",\"count\":" + String(count) + "}";

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  http.end();

  Serial.print(json);
  Serial.print("  -> response: ");
  Serial.println(code);   // 200 = server got it, -1 = couldn't reach server

  delay(5000);
}

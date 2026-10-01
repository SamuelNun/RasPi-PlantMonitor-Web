#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  Serial.print("Hello from ESP32, MAC: ");
  Serial.println(WiFi.macAddress());
  delay(2000);
}

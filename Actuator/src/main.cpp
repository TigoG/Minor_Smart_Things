#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.printf("Got %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                len,
                mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  Serial.println("ESP-NOW ready");

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  delay(1000);
}

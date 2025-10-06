#include "CommManager.h"
#include "Common.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

CommManager::CommManager() {}

void CommManager::begin() {
  WiFi.mode(WIFI_STA);
  // Non-destructive disconnect: don't pass 'true' which can stop/deinit WiFi driver
  // and on some SDK versions may also de-initialize ESP-NOW.
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (!httpQueue) httpQueue = xQueueCreate(5, sizeof(sensor_payload_t));
  xTaskCreatePinnedToCore(&CommManager::taskEntry, "CommTask", 8192, this, 1, NULL, 1);
}

void CommManager::taskEntry(void* pv) {
  static_cast<CommManager*>(pv)->task();
}

String CommManager::makeJson(const sensor_payload_t &p) {
  String body = "{";
  if (!isnan(p.tempC)) body += "\"temp\":" + String(p.tempC, 1); else body += "\"temp\":null";
  body += ",\"humidity\":";
  if (!isnan(p.humidity)) body += String(p.humidity, 1); else body += "null";
  body += ",\"lux\":";
  if (!isnan(p.lux)) body += String(p.lux, 1); else body += "null";
  body += ",\"wind_kmh\":";
  body += String(p.wind_kmh, 2);
  body += ",\"seq\":";
  body += String(p.seq);
  body += "}";
  return body;
}

void CommManager::task() {
  sensor_payload_t payload;
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected (CommManager will retry on send)");
  }

  for (;;) {
    if (httpQueue && xQueueReceive(httpQueue, &payload, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Comm: WiFi not connected, attempting reconnect...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
          vTaskDelay(pdMS_TO_TICKS(200));
        }
      }
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(SERVER_URL);
        http.addHeader("Content-Type", "application/json");
        String body = makeJson(payload);
        int code = http.POST(body);
        if (code > 0) {
          String resp = http.getString();
          Serial.printf("HTTP %d: %s\n", code, resp.c_str());
        } else {
          Serial.printf("HTTP POST failed, err=%d\n", code);
        }
        http.end();
      } else {
        Serial.println("Comm: still not connected, skipping HTTP POST");
      }
    }
  }
}
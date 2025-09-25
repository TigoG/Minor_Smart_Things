#include "managers/EspNowManager.h"
#include "managers/Common.h"
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspNowManager::EspNowManager() {}

void EspNowManager::begin() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    Serial.println("ESP-NOW initialized");
  }
  esp_now_register_send_cb(&EspNowManager::onDataSent);

  bool isBroadcast = true;
  for (int i = 0; i < 6; ++i) if (actuatorMac[i] != 0xFF) { isBroadcast = false; break; }
  if (!isBroadcast) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, actuatorMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add ESP-NOW peer");
    } else {
      Serial.println("ESP-NOW peer added");
    }
  } else {
    Serial.println("Using broadcast for ESP-NOW");
  }

  if (!espNowQueue) espNowQueue = xQueueCreate(5, sizeof(sensor_payload_t));
  xTaskCreatePinnedToCore(&EspNowManager::taskEntry, "EspNowTask", 4096, this, 2, NULL, 1);
}

void EspNowManager::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void EspNowManager::taskEntry(void* pv) {
  static_cast<EspNowManager*>(pv)->task();
}

void EspNowManager::task() {
  sensor_payload_t payload;
  for (;;) {
    if (espNowQueue && xQueueReceive(espNowQueue, &payload, portMAX_DELAY) == pdTRUE) {
      bool isBroadcast = true;
      for (int i = 0; i < 6; ++i) if (actuatorMac[i] != 0xFF) { isBroadcast = false; break; }

      esp_err_t res;
      if (isBroadcast) {
        uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        res = esp_now_send(bcast, (uint8_t*)&payload, sizeof(payload));
      } else {
        res = esp_now_send(actuatorMac, (uint8_t*)&payload, sizeof(payload));
      }
      if (res != ESP_OK) {
        Serial.printf("esp_now_send err=%d\n", res);
      }
    }
  }
}
#include "EspNowManager.h"
#include "Common.h"
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Internal state
static volatile bool s_lastSendFailed = false;
static int s_peerChannel = -1;
static bool s_isBroadcast = true;

// Constructor
EspNowManager::EspNowManager() {}

// Initialize ESP-NOW
void EspNowManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100)); // allow hardware to settle

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return; // stop here if initialization fails
    }
    Serial.println("ESP-NOW initialized");

    esp_now_register_send_cb(&EspNowManager::onDataSent);

    s_isBroadcast = true;
    for (int i = 0; i < 6; ++i)
        if (actuatorMac[i] != 0xFF) { s_isBroadcast = false; break; }

    if (!s_isBroadcast) {
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, actuatorMac, 6);
        // Use current WiFi channel (fallback to 1 if unknown)
        uint8_t ch = WiFi.channel();
        if (ch == 0) ch = 1;
        peerInfo.channel = ch;
        peerInfo.encrypt = false;

        esp_err_t res = esp_now_add_peer(&peerInfo);
        if (res != ESP_OK) {
            Serial.printf("Failed to add ESP-NOW peer (err=%d)\n", res);
            s_peerChannel = -1;
        } else {
            Serial.printf("ESP-NOW peer added on channel %d\n", peerInfo.channel);
            s_peerChannel = peerInfo.channel;
        }
    } else {
        Serial.println("Using broadcast for ESP-NOW");
        s_peerChannel = -1;
    }

    if (!espNowQueue) {
        espNowQueue = xQueueCreate(5, sizeof(sensor_payload_t));
    }
    xTaskCreatePinnedToCore(&EspNowManager::taskEntry, "EspNowTask", 4096, this, 2, NULL, 1);
}

// Send callback
void EspNowManager::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("ESP-NOW send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    s_lastSendFailed = (status != ESP_NOW_SEND_SUCCESS);
}

// Task entry
void EspNowManager::taskEntry(void* pv) {
    static_cast<EspNowManager*>(pv)->task();
}

// Main task loop
void EspNowManager::task() {
    sensor_payload_t payload;
    for (;;) {
        if (espNowQueue && xQueueReceive(espNowQueue, &payload, portMAX_DELAY) == pdTRUE) {
            // If we have a specific peer, ensure its channel matches current WiFi channel.
            if (!s_isBroadcast) {
                uint8_t currentCh = WiFi.channel();
                if (currentCh == 0) currentCh = 1;
                if (currentCh != s_peerChannel) {
                    Serial.printf("WiFi channel changed (%d != %d), updating ESP-NOW peer\n", currentCh, s_peerChannel);
                    esp_err_t delRes = esp_now_del_peer(actuatorMac);
                    if (delRes != ESP_OK) {
                        Serial.printf("esp_now_del_peer returned %d (continuing)\n", delRes);
                    }
                    esp_now_peer_info_t peerInfo;
                    memset(&peerInfo, 0, sizeof(peerInfo));
                    memcpy(peerInfo.peer_addr, actuatorMac, 6);
                    peerInfo.channel = currentCh;
                    peerInfo.encrypt = false;
                    esp_err_t addRes = esp_now_add_peer(&peerInfo);
                    if (addRes == ESP_OK) {
                        Serial.printf("ESP-NOW peer re-added on channel %d\n", currentCh);
                        s_peerChannel = currentCh;
                    } else {
                        Serial.printf("Failed to re-add ESP-NOW peer on channel %d (err=%d)\n", currentCh, addRes);
                    }
                }
            }

            esp_err_t res;
            if (s_isBroadcast) {
                uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
                res = esp_now_send(bcast, (uint8_t*)&payload, sizeof(payload));
            } else {
                res = esp_now_send(actuatorMac, (uint8_t*)&payload, sizeof(payload));
                if (res != ESP_OK) {
                    Serial.printf("ESP-NOW send failed (err=%d), attempting to re-add peer and resend\n", res);
                    esp_now_peer_info_t peerInfo;
                    memset(&peerInfo, 0, sizeof(peerInfo));
                    memcpy(peerInfo.peer_addr, actuatorMac, 6);
                    uint8_t ch = WiFi.channel(); if (ch == 0) ch = 1;
                    peerInfo.channel = ch;
                    peerInfo.encrypt = false;
                    esp_err_t add = esp_now_add_peer(&peerInfo);
                    if (add == ESP_OK) {
                        s_peerChannel = ch;
                        res = esp_now_send(actuatorMac, (uint8_t*)&payload, sizeof(payload));
                    } else {
                        Serial.printf("esp_now_add_peer failed during resend (err=%d)\n", add);
                    }
                }
            }

            if (res != ESP_OK) {
                Serial.printf("ESP-NOW send failed final (err=%d)\n", res);
            }
        }
    }
}

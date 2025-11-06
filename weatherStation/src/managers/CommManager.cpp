#include "CommManager.h"
#include "Common.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "secret.h"

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static const char* MQTT_BROKER = "145.24.237.211";
static const uint16_t MQTT_PORT = 8883;
static const char* MQTT_TOPIC_BASE = "homestations/1051804/0";

CommManager::CommManager() {}

void CommManager::begin() {
  WiFi.mode(WIFI_STA);
  // Non-destructive disconnect: don't pass 'true' which can stop/deinit WiFi driver
  // and on some SDK versions may also de-initialize ESP-NOW.
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // configure MQTT server
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

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
  if (!isnan(p.wind_kmh)) body += String(p.wind_kmh, 1); else body += "null";
  body += ",\"seq\":";
  body += String(p.seq);
  body += "}";
  return body;
}

void CommManager::task() {
  sensor_payload_t payload;
  unsigned long start = millis();
  unsigned long lastStatus = 0;
  unsigned long lastMqttReconnect = 0;

  // Initial connect attempt with periodic status prints (every 5s)
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    if (millis() - lastStatus >= 5000) {
      int st = WiFi.status();
      const char* ststr;
      switch (st) {
        case WL_CONNECTED: ststr = "CONNECTED"; break;
        case WL_IDLE_STATUS: ststr = "IDLE"; break;
        case WL_NO_SSID_AVAIL: ststr = "NO_SSID_AVAIL"; break;
        case WL_CONNECT_FAILED: ststr = "CONNECT_FAILED"; break;
        case WL_CONNECTION_LOST: ststr = "CONNECTION_LOST"; break;
        case WL_DISCONNECTED: ststr = "DISCONNECTED"; break;
        default: ststr = "UNKNOWN"; break;
      }
      Serial.printf("WiFi status: %s\n", ststr);
      lastStatus = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    lastStatus = millis();
  } else {
    Serial.println("WiFi not connected (CommManager will retry on send)");
    lastStatus = millis();
  }

  for (;;) {
    // Wait up to 5s for an outgoing payload so we can print status periodically
    BaseType_t got = pdFALSE;
    if (httpQueue) got = xQueueReceive(httpQueue, &payload, pdMS_TO_TICKS(5000));

    unsigned long now = millis();
    if (now - lastStatus >= 5000) {
      int st = WiFi.status();
      const char* ststr;
      switch (st) {
        case WL_CONNECTED: ststr = "CONNECTED"; break;
        case WL_IDLE_STATUS: ststr = "IDLE"; break;
        case WL_NO_SSID_AVAIL: ststr = "NO_SSID_AVAIL"; break;
        case WL_CONNECT_FAILED: ststr = "CONNECT_FAILED"; break;
        case WL_CONNECTION_LOST: ststr = "CONNECTION_LOST"; break;
        case WL_DISCONNECTED: ststr = "DISCONNECTED"; break;
        default: ststr = "UNKNOWN"; break;
      }
      if (st == WL_CONNECTED) {
        Serial.printf("WiFi status: %s, IP: %s\n", ststr, WiFi.localIP().toString().c_str());
      } else {
        Serial.printf("WiFi status: %s\n", ststr);
      }
      lastStatus = now;
    }

    // Maintain MQTT connection when WiFi is available
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttClient.connected()) {
        // Try to reconnect no more than once every 5s
        if (now - lastMqttReconnect >= 5000) {
          char clientId[48];
          String mac = WiFi.macAddress();
          String id = "ws-";
          id += mac;
          id.toCharArray(clientId, sizeof(clientId));
          if (mqttClient.connect(clientId, secret::MQTT_USER, secret::MQTT_PASS)) {
            Serial.println("MQTT connected");
            mqttClient.publish("homestations/1051804/0/gps", "51.5040, 3.8880");
          } else {
            Serial.printf("MQTT connect failed, rc=%d\n", mqttClient.state());
          }
          lastMqttReconnect = now;
        }
      } else {
        mqttClient.loop();
      }
    }

    if (got == pdTRUE) {
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
        // Ensure MQTT connected before publish; attempt immediate connect if not
        if (!mqttClient.connected()) {
          char clientId[48];
          String mac = WiFi.macAddress();
          String id = "ws-";
          id += mac;
          id.toCharArray(clientId, sizeof(clientId));
          if (mqttClient.connect(clientId, secret::MQTT_USER, secret::MQTT_PASS)) {
            Serial.println("MQTT connected (on send)");
            mqttClient.publish("homestations/1051804/0/gps", "51.5040, 3.8880");
          } else {
            Serial.printf("MQTT connect failed (on send) rc=%d\n", mqttClient.state());
          }
        }

        if (mqttClient.connected()) {
          char topic[64];
          char msgbuf[32];

          if (!isnan(payload.tempC)) {
            dtostrf(payload.tempC, 0, 1, msgbuf);
            snprintf(topic, sizeof(topic), "%s/temperature", MQTT_TOPIC_BASE);
            if (!mqttClient.publish(topic, msgbuf)) {
              Serial.println("MQTT publish Temperature failed");
            }
          }

          if (!isnan(payload.humidity)) {
            dtostrf(payload.humidity, 0, 1, msgbuf);
            snprintf(topic, sizeof(topic), "%s/humidity", MQTT_TOPIC_BASE);
            mqttClient.publish(topic, msgbuf);
          }

          // Wind speed in km/h (calibrated)
          if (!isnan(payload.wind_kmh)) {
            dtostrf(payload.wind_kmh, 0, 1, msgbuf);
          } else {
            snprintf(msgbuf, sizeof(msgbuf), "null");
          }
          snprintf(topic, sizeof(topic), "%s/windspeed", MQTT_TOPIC_BASE);
          mqttClient.publish(topic, msgbuf);

          if (!isnan(payload.lux)) {
            dtostrf(payload.lux, 0, 1, msgbuf);
            snprintf(topic, sizeof(topic), "%s/light", MQTT_TOPIC_BASE);
            mqttClient.publish(topic, msgbuf);
          }

          // Optionally publish sequence
          snprintf(topic, sizeof(topic), "%s/Seq", MQTT_TOPIC_BASE);
          snprintf(msgbuf, sizeof(msgbuf), "%lu", (unsigned long)payload.seq);
          mqttClient.publish(topic, msgbuf);

          mqttClient.publish("homestations/1051804/0/update", "update");

          mqttClient.loop();
        } else {
          Serial.println("Comm: MQTT not connected, skipping MQTT publish");
        }

        // Also send to HTTP endpoint if configured (backwards compatibility)
        if (SERVER_URL && SERVER_URL[0] != '\0') {
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
          //Serial.println("Comm: SERVER_URL empty, skipping HTTP POST");
        }
      } else {
        Serial.println("Comm: still not connected, skipping network send");
      }
    }
  }
}
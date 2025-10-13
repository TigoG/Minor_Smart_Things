#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "ShadeController.h"
#include <stdlib.h>
#include <string.h>

// Default pin for MG90 servo
const int SERVO_PIN = 5; // change to match your wiring

// Default motion parameters
const float DEFAULT_ANGLE = 45.0f;
const unsigned long DEFAULT_UP_DURATION = 5000UL;   // 5s
const unsigned long DEFAULT_DOWN_DURATION = 10000UL; // 10s

ShadeController *gShadeController = nullptr; // define the extern

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  HELP");
  Serial.println("  SENSOR <tempC|NaN> <humidity|NaN> <lux|NaN> <wind_kmh|NaN> <seq>");
  Serial.println("  UP [angle] [duration_ms]");
  Serial.println("  DOWN [angle] [duration_ms]");
  Serial.println("  STATUS");
}

void processSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  char buf[256];
  line.toCharArray(buf, sizeof(buf));
  char *tok = strtok(buf, " \t");
  if (!tok) return;
  String cmd = String(tok);
  cmd.toUpperCase();

  if (cmd == "HELP") { printHelp(); return; }

  if (cmd == "SENSOR") {
    if (!gShadeController) { Serial.println("No ShadeController instance"); return; }
    float temp = NAN, hum = NAN, lux = NAN, wind = NAN;
    uint32_t seq = 0;
    tok = strtok(NULL, " \t"); if (tok) temp = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
    tok = strtok(NULL, " \t"); if (tok) hum  = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
    tok = strtok(NULL, " \t"); if (tok) lux  = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
    tok = strtok(NULL, " \t"); if (tok) wind = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
    tok = strtok(NULL, " \t"); if (tok) seq = (uint32_t)strtoul(tok, NULL, 10);

    SensorPayload payload;
    payload.tempC = temp;
    payload.humidity = hum;
    payload.lux = lux;
    payload.wind_kmh = wind;
    payload.seq = seq;

    Serial.printf("Injecting sensor: temp=%0.1f hum=%0.1f lux=%0.1f wind=%0.2f seq=%u\n",
                  payload.tempC, payload.humidity, payload.lux, payload.wind_kmh, payload.seq);

    gShadeController->handleMessage((const uint8_t *)&payload, sizeof(payload));
    return;
  }

  if (cmd == "UP" || cmd == "DOWN") {
    float angle = DEFAULT_ANGLE;
    unsigned long duration = (cmd == "UP") ? DEFAULT_UP_DURATION : DEFAULT_DOWN_DURATION;
    tok = strtok(NULL, " \t"); if (tok) angle = atof(tok);
    tok = strtok(NULL, " \t"); if (tok) duration = (unsigned long)strtoul(tok, NULL, 10);

    String payload = (cmd == "UP") ? "up" : "down";
    payload += " angle:";
    payload += String(angle, 1);
    payload += " duration:";
    payload += String(duration);

    Serial.printf("Injecting command: %s\n", payload.c_str());
    if (gShadeController) gShadeController->handleMessage((const uint8_t *)payload.c_str(), payload.length());
    return;
  }

  if (cmd == "STATUS") {
    Serial.println("ShadeController configured.");
    return;
  }

  Serial.printf("Unknown command: %s\n", cmd.c_str());
  printHelp();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // create controller instance with servo pin
  gShadeController = new ShadeController(SERVO_PIN, DEFAULT_ANGLE, DEFAULT_UP_DURATION, DEFAULT_DOWN_DURATION);
  gShadeController->begin();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    Serial.println("ESP-NOW ready");
    esp_now_register_recv_cb(onDataRecv);
  }

  Serial.println("Ready. Type HELP for commands.");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    processSerialLine(line);
  }
  delay(10);
}

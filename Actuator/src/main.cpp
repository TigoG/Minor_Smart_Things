#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "StepperController.h"
#include "ShadeController.h"
#include <stdlib.h>
#include <string.h>

// Pins for A4988 + NEMA17 - change to match your wiring
const int STEP_PIN = 27;
const int DIR_PIN = 26;
const int ENABLE_PIN = 14; // set to -1 if not used

// Default motion parameters
const float DEFAULT_ANGLE = 45.0f;
const unsigned long DEFAULT_UP_DURATION = 5000UL;   // 5s
const unsigned long DEFAULT_DOWN_DURATION = 10000UL; // 10s

// Global stepper instance (constructed with NEMA17/A4988 defaults)
// Default to simulation mode so you can test without a motor power supply.
// Toggle at runtime with "SIM ON" / "SIM OFF" from the serial monitor.
StepperController stepper(STEP_PIN, DIR_PIN, ENABLE_PIN, 200, 16, 1.0f, 5, true);
ShadeController *gShadeController = nullptr;

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  HELP");
  Serial.println("  SIM ON|OFF");
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

  if (cmd == "HELP") {
    printHelp();
    return;
  }

  if (cmd == "SIM" || cmd == "SIMULATE") {
    tok = strtok(NULL, " \t");
    if (!tok) { Serial.println("Usage: SIM ON|OFF"); return; }
    String arg = String(tok); arg.toUpperCase();
    bool on = (arg == "ON" || arg == "1" || arg == "TRUE");
    stepper.setSimulate(on);
    Serial.printf("Simulation %s\n", on ? "ENABLED" : "DISABLED");
    return;
  }

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
                  payload.tempC,
                  payload.humidity,
                  payload.lux,
                  payload.wind_kmh,
                  payload.seq);

    // feed binary payload directly to ShadeController (it will decode and act)
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
    Serial.printf("Simulation: %s\n", stepper.isSimulating() ? "ENABLED" : "DISABLED");
    return;
  }

  Serial.printf("Unknown command: %s\n", cmd.c_str());
  printHelp();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  stepper.begin();

  // create controller instance
  gShadeController = new ShadeController(stepper, DEFAULT_ANGLE, DEFAULT_UP_DURATION, DEFAULT_DOWN_DURATION);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    Serial.println("ESP-NOW ready");
    esp_now_register_recv_cb(onDataRecv);
  }

  Serial.println("Ready. Type HELP for commands. Simulation enabled by default.");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    processSerialLine(line);
  }

  // Keep loop lightweight; actions are driven by ESP-NOW or serial injection
  delay(10);
}

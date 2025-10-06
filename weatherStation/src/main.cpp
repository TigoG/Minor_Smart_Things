#include <Arduino.h>
#include <Wire.h>
#include "Common.h"
#include "SensorManager.h"
#include "EspNowManager.h"
#include "CommManager.h"
#include "DisplayManager.h"
#include "secret.h"

// Global objects declared in Common.h
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

QueueHandle_t espNowQueue = NULL;
QueueHandle_t httpQueue = NULL;
QueueHandle_t displayQueue = NULL;

// Networking placeholders
const char* WIFI_SSID = secret::WIFI_SSID;
const char* WIFI_PASS = secret::WIFI_PASS;
const char* SERVER_URL = secret::SERVER_URL;

// ESP-NOW peer
uint8_t actuatorMac[6] = {0x70, 0xB8, 0xF6, 0x5D, 0x12, 0xCC};

// Manager instances
static SensorManager* gSensorManager = nullptr;
static EspNowManager* gEspNowManager = nullptr;
static CommManager* gCommManager = nullptr;
static DisplayManager* gDisplayManager = nullptr;

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(100)); // allow serial to start

  // instantiate managers
  gDisplayManager = new DisplayManager();
  gEspNowManager = new EspNowManager();
  gCommManager = new CommManager();
  gSensorManager = new SensorManager();

  // Start components
  gDisplayManager->begin(); // start display first for boot messages
  gEspNowManager->begin();
  gCommManager->begin();
  gSensorManager->begin();
}

void loop() {
  // All work is performed in FreeRTOS tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}

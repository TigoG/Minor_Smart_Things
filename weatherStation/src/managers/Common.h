#ifndef MANAGERS_COMMON_H
#define MANAGERS_COMMON_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Display config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// Pins and hardware defines
#define DHTPIN 4
#define DHTTYPE DHT22
#define HALL_PIN 15
#define SDA_PIN 21
#define SCL_PIN 22

// Anemometer params
static constexpr float ANEMOMETER_RADIUS_M = 0.09f;
static constexpr uint8_t PULSES_PER_REV = 1;

// BH1750
static constexpr uint8_t BH1750_ADDR = 0x23;
static constexpr uint8_t BH1750_ONE_TIME_HIGH_RES_MODE = 0x20;

// Timing
static constexpr unsigned long MEAS_INTERVAL_MS = 2000;

// Payload
typedef struct __attribute__((packed)) {
  float tempC;
  float humidity;
  float lux;
  float wind_kmh;
  uint32_t seq;
} sensor_payload_t;

// Queues (defined in main.cpp)
extern QueueHandle_t espNowQueue;
extern QueueHandle_t httpQueue;
extern QueueHandle_t displayQueue;

// Display object (defined in main.cpp)
extern Adafruit_SSD1306 display;

// Network/config (defined in main.cpp)
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;
extern const char* SERVER_URL;
extern uint8_t actuatorMac[6];

#endif // MANAGERS_COMMON_H
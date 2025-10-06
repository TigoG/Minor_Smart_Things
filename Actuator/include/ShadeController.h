#ifndef SHADE_CONTROLLER_H
#define SHADE_CONTROLLER_H

#include <Arduino.h>
#include "StepperController.h"
#include <stdint.h>

// Sensor payload layout shared with weatherStation
// typedef struct __attribute__((packed)) { float tempC; float humidity; float lux; float wind_kmh; uint32_t seq; } SensorPayload;
typedef struct __attribute__((packed)) {
  float tempC;
  float humidity;
  float lux;
  float wind_kmh;
  uint32_t seq;
} SensorPayload;

class ShadeController {
public:
  ShadeController(StepperController &stepper,
                  float defaultAngle = 45.0f,
                  unsigned long upDuration = 5000UL,
                  unsigned long downDuration = 10000UL);

  ~ShadeController();

  void handleMessage(const uint8_t *data, int len);

private:
  StepperController &stepper_;
  float defaultAngle_;
  unsigned long upDuration_;
  unsigned long downDuration_;

  void performUp(float angle, unsigned long durationMs);
  void performDown(float angle, unsigned long durationMs);
};

extern ShadeController *gShadeController;

// ESP-NOW receive callback (forward-declared so main can register it)
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

#endif // SHADE_CONTROLLER_H
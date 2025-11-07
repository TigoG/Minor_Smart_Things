#ifndef SHADE_CONTROLLER_H
#define SHADE_CONTROLLER_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <stdint.h>

// Sensor payload layout shared with weatherStation
typedef struct __attribute__((packed)) {
  float tempC;
  float humidity;
  float lux;
  float wind_kmh;
  uint32_t seq;
} SensorPayload;

class ShadeController {
public:
  ShadeController(int servoPin,
                  float defaultAngle = 90.0f,
                  unsigned long upDuration = 5000UL,
                  unsigned long downDuration = 10000UL);

  ~ShadeController();

  // Attach servo and perform any initialization
  void begin();
///////
// Handle incoming messages (ASCII or binary SensorPayload)
  void handleMessage(const uint8_t *data, int len);

  // Pulse to an angle, hold for holdMs milliseconds, then return to closed (0).
  // moveDurationMs specifies how long the motion to/from the target should take (ms).
  void pulseAngle(float angleDeg, unsigned long holdMs, unsigned long moveDurationMs = 500UL);

private:
  Servo _servo;
  int _servoPin;
  float _currentAngle;
  float _defaultAngle;
  unsigned long _upDuration;
  unsigned long _downDuration;

  void setServoAngle(float angleDeg);
  void performUp(float angle, unsigned long durationMs);
  void performDown(float angle, unsigned long durationMs);
};

extern ShadeController *gShadeController;

// ESP-NOW receive callback (forward-declared so main can register it)
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

#endif // SHADE_CONTROLLER_H
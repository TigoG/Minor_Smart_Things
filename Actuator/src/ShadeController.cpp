#include "ShadeController.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

static float parseNumericValue(const String &lowerPayload, const String &key, float fallback) {
  int idx = lowerPayload.indexOf(key);
  if (idx < 0) return fallback;
  idx += key.length();
  while (idx < lowerPayload.length() && !isDigit(lowerPayload[idx]) && lowerPayload[idx] != '-' && lowerPayload[idx] != '.') idx++;
  String num;
  while (idx < lowerPayload.length() && (isDigit(lowerPayload[idx]) || lowerPayload[idx] == '.' || lowerPayload[idx] == '-')) {
    num += lowerPayload[idx++];
  }
  if (num.length()) return num.toFloat();
  return fallback;
}

/* Shade state and thresholds ------------------------------------------------- */
enum ShadeState { SHADE_CLOSED = 0, SHADE_OPEN = 1, SHADE_MOVING = 2, SHADE_UNKNOWN = 3 };
static ShadeState s_shadeState = SHADE_CLOSED; // start closed by default

static const float CLOSE_TEMP_THRESHOLD = 15.0f;
static const float CLOSE_LUX_THRESHOLD  = 15.0f;
static const float OPEN_TEMP_THRESHOLD  = 23.0f;
static const float OPEN_LUX_THRESHOLD   = 75.0f;

// Simple lock to avoid rapid re-triggering when sensors fluctuate
static const unsigned long STATE_CHANGE_LOCK_MS = 5000UL;
static unsigned long s_lastActionMillis = 0;

/* Constructor ----------------------------------------------------------------*/
ShadeController::ShadeController(int servoPin,
                                 float defaultAngle,
                                 unsigned long upDuration,
                                 unsigned long downDuration)
  : _servoPin(servoPin),
    _currentAngle(0.0f),
    _defaultAngle(defaultAngle),
    _upDuration(upDuration),
    _downDuration(downDuration) {}

ShadeController::~ShadeController() {
  _servo.detach();
}

void ShadeController::begin() {
  _servo.attach(_servoPin);
  setServoAngle(0.0f); // start closed
  Serial.printf("ShadeController: servo attached to pin %d\n", _servoPin);
}

void ShadeController::setServoAngle(float angleDeg) {
  if (angleDeg < 0.0f) angleDeg = 0.0f;
  if (angleDeg > 180.0f) angleDeg = 180.0f;
  _servo.write((int)round(angleDeg));
  _currentAngle = angleDeg;
  delay(20); // let servo start moving
}

void ShadeController::handleMessage(const uint8_t *data, int len) {
  if (len <= 0 || data == nullptr) return;

  // Check printable
  bool printable = true;
  for (int i = 0; i < len; ++i) {
    if (data[i] < 32 || data[i] > 126) { printable = false; break; }
  }

  float angle = _defaultAngle;
  unsigned long duration = _upDuration;
  bool doUp = false;
  bool doDown = false;

  if (printable) {
    String payload = String((const char *)data, len);
    payload.trim();
    String lower = payload;
    lower.toLowerCase();
    Serial.println("Payload: " + payload);

    if (lower.indexOf("up") != -1 || lower.indexOf("open") != -1 || lower.indexOf("\"command\":\"up\"") != -1) doUp = true;
    if (lower.indexOf("down") != -1 || lower.indexOf("close") != -1 || lower.indexOf("\"command\":\"down\"") != -1) doDown = true;

    angle = parseNumericValue(lower, "angle", angle);
    float durF = parseNumericValue(lower, "duration", (float)duration);
    if (durF > 0) duration = (unsigned long)durF;

    int idx = lower.indexOf("light");
    if (idx >= 0) {
      float lightVal = parseNumericValue(lower, "light", -1.0f);
      Serial.printf("Light sensor: %.1f\n", lightVal);
      if (lightVal >= 0) {
        if (!doUp && !doDown) {
          if (lightVal >= 800) doDown = true;
          else if (lightVal <= 300) doUp = true;
        }
      }
    }

    if (doUp && doDown) {
      Serial.println("Ambiguous command: contains both up and down; ignoring.");
      return;
    }

    if (doUp) {
      performUp(angle, duration);
    } else if (doDown) {
      performDown(angle, duration);
    } else {
      Serial.println("No actionable command in payload; ignoring.");
    }
  } else {
    // Binary payloads: interpret as SensorPayload (from weatherStation) or fallback to simple commands
    Serial.println("Binary payload received");

    if (len >= (int)sizeof(SensorPayload)) {
      SensorPayload payload;
      memcpy(&payload, data, sizeof(SensorPayload));

      Serial.printf("Sensor: seq=%u temp=%0.1f hum=%0.1f wind=%0.2f lux=%0.1f\n",
                    payload.seq,
                    isnan(payload.tempC) ? NAN : payload.tempC,
                    isnan(payload.humidity) ? NAN : payload.humidity,
                    payload.wind_kmh,
                    isnan(payload.lux) ? NAN : payload.lux);

      bool tempValid = !isnan(payload.tempC);
      bool luxValid = !isnan(payload.lux);

      if (tempValid || luxValid) {
        bool triggerOpen = false;
        bool triggerClose = false;

        if (tempValid) {
          if (payload.tempC <= CLOSE_TEMP_THRESHOLD) triggerClose = true;
          if (payload.tempC >= OPEN_TEMP_THRESHOLD)  triggerOpen  = true;
        }
        if (luxValid) {
          if (payload.lux <= CLOSE_LUX_THRESHOLD) triggerClose = true;
          if (payload.lux >= OPEN_LUX_THRESHOLD)  triggerOpen  = true;
        }

        if (triggerOpen && triggerClose) {
          Serial.println("Policy: ambiguous open+close triggers - ignoring.");
        } else if (triggerClose) {
          if (s_shadeState == SHADE_CLOSED) {
            Serial.println("Policy: already CLOSED - no action taken.");
          } else if (millis() - s_lastActionMillis < STATE_CHANGE_LOCK_MS) {
            Serial.println("Policy: action locked - ignoring rapid changes.");
          } else {
            Serial.println("Policy: CLOSE triggered -> performing DOWN");
            performDown(_defaultAngle, _downDuration);
          }
        } else if (triggerOpen) {
          if (s_shadeState == SHADE_OPEN) {
            Serial.println("Policy: already OPEN - no action taken.");
          } else if (millis() - s_lastActionMillis < STATE_CHANGE_LOCK_MS) {
            Serial.println("Policy: action locked - ignoring rapid changes.");
          } else {
            Serial.println("Policy: OPEN triggered -> performing UP");
            performUp(_defaultAngle, _upDuration);
          }
        } else {
          Serial.println("Policy: sensors present but do not trigger action.");
        }
      } else {
        Serial.println("Policy: temp and lux missing - ignoring sensor-based decision.");
      }
    } else {
      if (data[0] == 1 || data[0] == 'U' || data[0] == 'u') {
        duration = _upDuration;
        performUp(angle, duration);
      } else if (data[0] == 2 || data[0] == 'D' || data[0] == 'd') {
        duration = _downDuration;
        performDown(angle, duration);
      } else {
        Serial.printf("Unknown binary command: 0x%02X\n", data[0]);
      }
    }
  }
}

void ShadeController::performUp(float angle, unsigned long durationMs) {
  if (s_shadeState == SHADE_OPEN) {
    Serial.println("performUp: already OPEN - skipping");
    return;
  }
  s_shadeState = SHADE_MOVING;
  Serial.println("performUp: moving to OPEN");

  float target = angle;
  if (target < 0.0f) target = 0.0f;
  if (target > 180.0f) target = 180.0f;

  float start = _currentAngle;
  int steps = 20;
  if (durationMs == 0) durationMs = 500;
  unsigned long stepDelay = durationMs / (unsigned long)steps;

  for (int i = 1; i <= steps; ++i) {
    float a = start + ((target - start) * (float)i / (float)steps);
    setServoAngle(a);
    if (stepDelay >= 1) delay(stepDelay);
  }

  s_shadeState = SHADE_OPEN;
  s_lastActionMillis = millis();
  Serial.println("performUp: complete, state=OPEN");
}

void ShadeController::performDown(float angle, unsigned long durationMs) {
  if (s_shadeState == SHADE_CLOSED) {
    Serial.println("performDown: already CLOSED - skipping");
    return;
  }
  s_shadeState = SHADE_MOVING;
  Serial.println("performDown: moving to CLOSED");

  float target = 0.0f; // closed position
  float start = _currentAngle;
  int steps = 20;
  if (durationMs == 0) durationMs = 500;
  unsigned long stepDelay = durationMs / (unsigned long)steps;

  for (int i = 1; i <= steps; ++i) {
    float a = start + ((target - start) * (float)i / (float)steps);
    setServoAngle(a);
    if (stepDelay >= 1) delay(stepDelay);
  }

  s_shadeState = SHADE_CLOSED;
  s_lastActionMillis = millis();
  Serial.println("performDown: complete, state=CLOSED");
}

// ESP-NOW receive callback
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.printf("Got %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                len,
                mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
  if (gShadeController) gShadeController->handleMessage(data, len);
}
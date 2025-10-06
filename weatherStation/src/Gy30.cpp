/*
 * src/Gy30.cpp
 * Deprecated example sketch for BH1750 (GY-30).
 * Functionality moved into managers/SensorManager.cpp.
 * This file kept as a lightweight helper to prevent duplicate
 * setup()/loop() symbols and to provide an optional helper.
 */

#include "Common.h"
#include <Wire.h>

/*
 * readGy30Lux()
 * Non-blocking helper to read two bytes from BH1750 and convert to lux.
 * Note: SensorManager initializes the BH1750 in continuous high-res mode.
 */
float readGy30Lux() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.endTransmission(); // ensure device ready
  Wire.requestFrom(BH1750_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read();
    raw <<= 8;
    raw |= Wire.read();
    return raw / 1.2f;
  }
  return NAN;
}

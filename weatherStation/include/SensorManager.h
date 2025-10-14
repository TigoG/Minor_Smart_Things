#ifndef MANAGERS_SENSORMANAGER_H
#define MANAGERS_SENSORMANAGER_H

#include "Common.h"

class SensorManager {
public:
  explicit SensorManager(uint32_t intervalMs = MEAS_INTERVAL_MS);
  void begin();

private:
  uint32_t _intervalMs;
  uint32_t _seq;

  static void taskEntry(void* pv);
  void task();
  float readLuxBH1750();

  // Bit-banged DHT22 reader (implemented in the .cpp)
  bool readDHT22(float &tempC, float &humidity);
};

#endif // MANAGERS_SENSORMANAGER_H
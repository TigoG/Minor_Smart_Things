#ifndef MANAGERS_SENSORMANAGER_H
#define MANAGERS_SENSORMANAGER_H

#include "Common.h"
#include <DHT.h>

class SensorManager {
public:
  explicit SensorManager(uint32_t intervalMs = MEAS_INTERVAL_MS);
  void begin();

private:
  uint32_t _intervalMs;
  uint32_t _seq;
  DHT dht;

  static void taskEntry(void* pv);
  void task();
  float readLuxBH1750();
};

#endif // MANAGERS_SENSORMANAGER_H
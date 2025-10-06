#ifndef MANAGERS_COMMMANAGER_H
#define MANAGERS_COMMMANAGER_H

#include "Common.h"

class CommManager {
public:
  CommManager();
  void begin();

private:
  static void taskEntry(void* pv);
  void task();
  String makeJson(const sensor_payload_t &p);
};

#endif // MANAGERS_COMMMANAGER_H
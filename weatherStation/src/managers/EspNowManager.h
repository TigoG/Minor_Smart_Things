#ifndef MANAGERS_ESPNOWMANAGER_H
#define MANAGERS_ESPNOWMANAGER_H

#include "managers/Common.h"
#include <esp_now.h>

class EspNowManager {
public:
  EspNowManager();
  void begin();

private:
  static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
  static void taskEntry(void* pv);
  void task();
};

#endif // MANAGERS_ESPNOWMANAGER_H
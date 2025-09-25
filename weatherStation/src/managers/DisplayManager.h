#ifndef MANAGERS_DISPLAYMANAGER_H
#define MANAGERS_DISPLAYMANAGER_H

#include "managers/Common.h"

class DisplayManager {
public:
  DisplayManager();
  void begin();

private:
  static void taskEntry(void* pv);
  void task();
};

#endif // MANAGERS_DISPLAYMANAGER_H
#include "DisplayManager.h"
#include "Common.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

DisplayManager::DisplayManager() {}

void DisplayManager::begin() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  if (!displayQueue) displayQueue = xQueueCreate(1, sizeof(sensor_payload_t));
  xTaskCreatePinnedToCore(&DisplayManager::taskEntry, "DisplayTask", 4096, this, 1, NULL, 1);
}

void DisplayManager::taskEntry(void* pv) {
  static_cast<DisplayManager*>(pv)->task();
}

void DisplayManager::task() {
  sensor_payload_t payload;
  for(;;) {
    if (displayQueue && xQueueReceive(displayQueue, &payload, portMAX_DELAY) == pdTRUE) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      if (!isnan(payload.tempC)) {
        display.print(payload.tempC, 1);
        display.print("C");
      } else {
        display.print("--C");
      }
      display.setTextSize(1);
      display.setCursor(0, 26);
      display.print("Hum:");
      if (!isnan(payload.humidity)) display.print(payload.humidity, 0); else display.print("--");
      display.print("%");
      display.setCursor(64, 26);
      display.print("Wind:");
      display.print(payload.wind_kmh, 1);
      display.print("km/h");
      display.setCursor(0, 44);
      display.setTextSize(1);
      // Clear the entire bottom area and print formatted lux field as a single string
      display.fillRect(0, 44, SCREEN_WIDTH, 20, SSD1306_BLACK);
      display.setTextSize(1);
      {
        char buf[24];
        if (!isnan(payload.lux)) {
          int lux_int = (int)(payload.lux + 0.5f);
          snprintf(buf, sizeof(buf), "Light: %d lx", lux_int);
        } else {
          snprintf(buf, sizeof(buf), "Light: -- lx");
        }
        display.setCursor(0, 44);
        display.print(buf);
      }
      display.display();
    }
  }
}
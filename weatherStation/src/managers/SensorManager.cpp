#include "SensorManager.h"
#include "Common.h"
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//
 // Local ISR state (kept private to this translation unit)
 //
 volatile uint32_t pulseCount = 0;
 volatile uint32_t lastPulseMicros = 0;
 
 IRAM_ATTR void hallISR() {
   uint32_t now = micros();
   if (now - lastPulseMicros > 1000) {
     pulseCount++;
     lastPulseMicros = now;
   }
 }

 SensorManager::SensorManager(uint32_t intervalMs)
   : _intervalMs(intervalMs), _seq(0) {}

 void SensorManager::begin() {
   Wire.begin(SDA_PIN, SCL_PIN);

   // Initialize BH1750 (GY-30) in continuous high-res mode
   Wire.beginTransmission(BH1750_ADDR);
   Wire.write(BH1750_CONTINUOUS_HIGH_RES_MODE);
   Wire.endTransmission();
   delay(200);

   // DHT22 handled by manual bit-banged reader; no library init required
   pinMode(HALL_PIN, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, FALLING);

   if (!espNowQueue) espNowQueue = xQueueCreate(5, sizeof(sensor_payload_t));
   if (!httpQueue) httpQueue = xQueueCreate(5, sizeof(sensor_payload_t));
   if (!displayQueue) displayQueue = xQueueCreate(1, sizeof(sensor_payload_t));

   xTaskCreatePinnedToCore(&SensorManager::taskEntry, "SensorTask", 4096, this, 2, NULL, 1);
 }

 void SensorManager::taskEntry(void* pv) {
   static_cast<SensorManager*>(pv)->task();
 }

 void SensorManager::task() {
   for (;;) {
     uint32_t start = millis();

     noInterrupts();
     uint32_t pulses = pulseCount;
     pulseCount = 0;
     interrupts();

     float revs = (float)pulses / (float)PULSES_PER_REV;
     float revs_per_sec = revs / (_intervalMs / 1000.0f);
     const float PI_f = 3.14159265358979323846f;
     float linear_speed_m_s = revs_per_sec * (2.0f * PI_f * ANEMOMETER_RADIUS_M);
     float wind_kmh = linear_speed_m_s * 3.6f;

     float tempC, humidity;
     if (!readDHT22(tempC, humidity)) {
       tempC = NAN;
       humidity = NAN;
     }
     float lux = readLuxBH1750();

     sensor_payload_t payload;
     payload.tempC = tempC;
     payload.humidity = humidity;
     payload.lux = lux;
     payload.wind_kmh = wind_kmh;
     payload.seq = ++_seq;

     Serial.printf("Sensor: seq=%u temp=%0.1f hum=%0.1f wind=%0.2f lux=%0.1f\n",
                   payload.seq,
                   isnan(payload.tempC) ? NAN : payload.tempC,
                   isnan(payload.humidity) ? NAN : payload.humidity,
                   payload.wind_kmh,
                   isnan(payload.lux) ? NAN : payload.lux);

     // Publish to queues (non-blocking)
     if (espNowQueue) xQueueSend(espNowQueue, &payload, 0);
     if (httpQueue) xQueueSend(httpQueue, &payload, 0);
     if (displayQueue) xQueueOverwrite(displayQueue, &payload);

     // Sleep until next measurement window
     uint32_t elapsed = millis() - start;
     if (elapsed < _intervalMs) vTaskDelay(pdMS_TO_TICKS(_intervalMs - elapsed));
   }
 }

 bool SensorManager::readDHT22(float &tempC, float &humidity) {
   uint8_t data[5] = {0,0,0,0,0};

   // Send start signal: pull low >800us, then release
   pinMode(DHTPIN, OUTPUT);
   digitalWrite(DHTPIN, LOW);
   delayMicroseconds(1100); // >800us to ensure sensor registers the start
   digitalWrite(DHTPIN, HIGH);
   delayMicroseconds(30);
   pinMode(DHTPIN, INPUT_PULLUP);

   noInterrupts();

   uint32_t t = micros();

   // Wait for sensor response: LOW (~80us)
   while (digitalRead(DHTPIN) == HIGH) {
     if (micros() - t > 1000) { interrupts(); pinMode(DHTPIN, INPUT_PULLUP); return false; }
   }

   // Wait for response: HIGH (~80us)
   t = micros();
   while (digitalRead(DHTPIN) == LOW) {
     if (micros() - t > 1000) { interrupts(); pinMode(DHTPIN, INPUT_PULLUP); return false; }
   }

   // Read 40 bits (5 bytes)
   for (int i = 0; i < 40; ++i) {
     // Wait for start of the bit (line goes LOW)
     t = micros();
     while (digitalRead(DHTPIN) == HIGH) {
       if (micros() - t > 1000) { interrupts(); pinMode(DHTPIN, INPUT_PULLUP); return false; }
     }

     // Wait for the line to go HIGH - start of the data bit
     t = micros();
     while (digitalRead(DHTPIN) == LOW) {
       if (micros() - t > 1000) { interrupts(); pinMode(DHTPIN, INPUT_PULLUP); return false; }
     }

     // Measure length of the HIGH signal; > ~40us means '1'
     uint32_t startHigh = micros();
     while (digitalRead(DHTPIN) == HIGH) {
       if (micros() - startHigh > 200) break;
     }
     uint32_t highLen = micros() - startHigh;

     data[i/8] <<= 1;
     if (highLen > 40) data[i/8] |= 1;
   }

   interrupts();

   // Validate checksum
   uint8_t sum = data[0] + data[1] + data[2] + data[3];
   if (sum != data[4]) {
     pinMode(DHTPIN, INPUT_PULLUP);
     return false;
   }

   uint16_t rawHumidity = ((uint16_t)data[0] << 8) | data[1];
   uint16_t rawTemp = ((uint16_t)data[2] << 8) | data[3];

   humidity = rawHumidity / 10.0f;
   if (rawTemp & 0x8000) {
     rawTemp &= 0x7FFF;
     tempC = -((int16_t)rawTemp) / 10.0f;
   } else {
     tempC = rawTemp / 10.0f;
   }

   pinMode(DHTPIN, INPUT_PULLUP);
   return true;
 }

 float SensorManager::readLuxBH1750() {
   // Read BH1750 in continuous high-res mode (initialized in begin())
   Wire.beginTransmission(BH1750_ADDR);
   Wire.endTransmission(); // ensure device ready
   Wire.requestFrom(BH1750_ADDR, (uint8_t)2);

   // 5-sample moving average with basic sanity/clamp checks
   static const int BUF_SIZE = 5;
   static float buf[BUF_SIZE] = {NAN, NAN, NAN, NAN, NAN};
   static int idx = 0;
   static int count = 0;
   static float sum = 0.0f;

   if (Wire.available() == 2) {
     uint16_t raw = Wire.read();
     raw <<= 8;
     raw |= Wire.read();
     float lux = raw / 1.2f;

     // Clamp to plausible BH1750 range
     if (isnan(lux) || lux < 0.0f || lux > 120000.0f) {
       // invalid reading, return previous average if available
       if (count > 0) return sum / (float)count;
       return NAN;
     }

     // basic spike rejection: if we have previous avg and new lux is > 10x avg and > 10000, ignore it
     if (count > 0) {
       float avg = sum / (float)count;
       if (lux > avg * 10.0f && lux > 10000.0f) {
         return avg;
       }
     }

     // add to circular buffer
     if (count < BUF_SIZE) {
       buf[idx] = lux;
       sum += lux;
       count++;
     } else {
       sum -= buf[idx];
       buf[idx] = lux;
       sum += lux;
     }
     idx = (idx + 1) % BUF_SIZE;
     return sum / (float)count;
   }

   // No data available: return previous average if any
   if (count > 0) return sum / (float)count;
   return NAN;
 }
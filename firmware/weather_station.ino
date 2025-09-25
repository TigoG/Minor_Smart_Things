/* 
  Minor Smart Things — Weather Station firmware (ESP32 Arduino)
  - DHT22 on GPIO16
  - BH1750 (I2C SDA=21,SCL=22)
  - Hall sensor (anemometer) on GPIO4
  - Gray-code wind direction on GPIO32,33,34,35,36,39
  - Sunshade servo on GPIO5 (50Hz via LEDC)
  - Outputs JSON over Serial; optional HTTP POST if WIFI configured

  Edit configuration constants below before compiling.
*/

#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --- Configuration (edit as needed) ---
// WiFi / server (leave empty if not using)
const char* WIFI_SSID = ""; // set your SSID
const char* WIFI_PASS = ""; // set your password
const char* SERVER_URL = ""; // optional HTTP endpoint to POST JSON, e.g. "http://192.168.1.100:5000/data"

// Pins (match design/wiring.md)
#define DHTPIN 16
#define DHTTYPE DHT22
const int HALL_PIN = 4;
const int DIR_PINS[6] = {32, 33, 34, 35, 36, 39}; // Gray-code bits (MSB -> LSB)
const int SERVO_PIN = 5; // PWM signal only; servo Vcc must be 5V
const int STATUS_LED_PIN = 2;

// BH1750 I2C address
const uint8_t BH1750_ADDR = 0x23;
const uint8_t BH1750_CONT_HIGHRES = 0x10;

// --- Defaults & calibration (persisted via Preferences) ---
const float DEFAULT_K_SPEED = 2.2f; // m per revolution (placeholder)
const int DEFAULT_PULSES_PER_REV = 2;
const float DEFAULT_LUX_CLOSE = 40000.0f;
const float DEFAULT_LUX_OPEN = 30000.0f;
const float DEFAULT_WIND_RETRACT = 10.0f; // m/s
const float DEFAULT_TEMP_CLOSE = 25.0f; // deg C
const int DEFAULT_SHADE_OPEN_ANGLE = 0;   // degrees
const int DEFAULT_SHADE_CLOSED_ANGLE = 90; // degrees
const unsigned long DEFAULT_MIN_SHADE_DWELL_MS = 60000UL; // 60 sec

// --- Runtime / state variables ---
DHT dht(DHTPIN, DHTTYPE);
Preferences prefs;

volatile unsigned long hallPulseCount = 0;
volatile unsigned long lastHallMicros = 0;
const unsigned long MIN_PULSE_INTERVAL_US = 5000UL; // 5 ms debounce in ISR

unsigned long lastHallCountSnapshot = 0;
unsigned long lastReportMs = 0;
const unsigned long REPORT_INTERVAL_MS = 10000UL; // report every 10 s

// configuration (loaded from prefs)
float k_speed;
int pulses_per_rev;
float lux_close_threshold;
float lux_open_threshold;
float wind_retract_threshold;
float temp_close_threshold;
int shade_open_angle;
int shade_closed_angle;
unsigned long min_shade_dwell_ms;

// shade state
int current_shade_angle = 0;
unsigned long last_shade_move_ms = 0;

// servo (LEDC)
const int SERVO_LEDC_CHANNEL = 0;
const int SERVO_LEDC_FREQ = 50;
const int SERVO_LEDC_RES = 16; // 16-bit resolution
const uint32_t SERVO_LEDC_MAX_DUTY = (1UL << SERVO_LEDC_RES) - 1;

// Gray-code buffer for smoothing direction
const int DIR_SMOOTH_SAMPLES = 6;
float dir_circle_x[DIR_SMOOTH_SAMPLES];
float dir_circle_y[DIR_SMOOTH_SAMPLES];
int dir_index = 0;
int dir_sample_count = 0;

// WiFi state
bool wifiConnected = false;

// --- Utility functions ---
void IRAM_ATTR hallISR() {
  unsigned long t = micros();
  if (t - lastHallMicros > MIN_PULSE_INTERVAL_US) {
    hallPulseCount++;
    lastHallMicros = t;
  }
}

unsigned int readGrayRaw() {
  unsigned int g = 0;
  for (int i = 0; i < 6; ++i) {
    int b = digitalRead(DIR_PINS[i]);
    g = (g << 1) | (b & 0x1);
  }
  return g;
}

unsigned int grayToBinary(unsigned int num) {
  unsigned int mask;
  for (mask = num >> 1; mask != 0; mask = mask >> 1) {
    num = num ^ mask;
  }
  return num;
}

float getDirectionDeg() {
  unsigned int gray = readGrayRaw();
  unsigned int bin = grayToBinary(gray);
  const int bits = 6;
  unsigned int maxCodes = 1U << bits;
  float deg = (float)bin * (360.0f / (float)maxCodes);
  // circular smoothing
  float rad = deg * PI / 180.0f;
  dir_circle_x[dir_index] = cos(rad);
  dir_circle_y[dir_index] = sin(rad);
  dir_index = (dir_index + 1) % DIR_SMOOTH_SAMPLES;
  if (dir_sample_count < DIR_SMOOTH_SAMPLES) dir_sample_count++;
  float sx = 0, sy = 0;
  for (int i = 0; i < dir_sample_count; ++i) { sx += dir_circle_x[i]; sy += dir_circle_y[i]; }
  float avg_rad = atan2(sy / dir_sample_count, sx / dir_sample_count);
  float avg_deg = avg_rad * 180.0f / PI;
  if (avg_deg < 0) avg_deg += 360.0f;
  return avg_deg;
}

// Minimal BH1750 driver (no external library)
void bh1750Init() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(BH1750_CONT_HIGHRES);
  Wire.endTransmission();
  delay(200);
}

float bh1750ReadLux() {
  Wire.requestFrom((int)BH1750_ADDR, 2);
  if (Wire.available() >= 2) {
    uint16_t high = Wire.read();
    uint16_t low = Wire.read();
    uint16_t raw = (high << 8) | low;
    float lux = raw / 1.2f; // per BH1750 datasheet
    return lux;
  }
  return -1.0f;
}

void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  // convert angle -> pulse width (500..2500 us typical)
  int us = map(angle, 0, 180, 500, 2500);
  // compute duty for 50Hz: duty = us / 20000us * maxDuty
  uint32_t duty = (uint32_t)((((float)us) / 20000.0f) * (float)SERVO_LEDC_MAX_DUTY);
  ledcWrite(SERVO_LEDC_CHANNEL, duty);
  current_shade_angle = angle;
  last_shade_move_ms = millis();
}

void loadConfig() {
  prefs.begin("ws_cfg", false);
  k_speed = prefs.getFloat("k_speed", DEFAULT_K_SPEED);
  pulses_per_rev = prefs.getInt("pulses_per_rev", DEFAULT_PULSES_PER_REV);
  lux_close_threshold = prefs.getFloat("lux_close", DEFAULT_LUX_CLOSE);
  lux_open_threshold = prefs.getFloat("lux_open", DEFAULT_LUX_OPEN);
  wind_retract_threshold = prefs.getFloat("wind_retract", DEFAULT_WIND_RETRACT);
  temp_close_threshold = prefs.getFloat("temp_close", DEFAULT_TEMP_CLOSE);
  shade_open_angle = prefs.getInt("shade_open_angle", DEFAULT_SHADE_OPEN_ANGLE);
  shade_closed_angle = prefs.getInt("shade_closed_angle", DEFAULT_SHADE_CLOSED_ANGLE);
  int dwell_s = prefs.getInt("shade_dwell_s", (int)(DEFAULT_MIN_SHADE_DWELL_MS / 1000UL));
  min_shade_dwell_ms = (unsigned long)dwell_s * 1000UL;
}

void saveConfig() {
  prefs.putFloat("k_speed", k_speed);
  prefs.putInt("pulses_per_rev", pulses_per_rev);
  prefs.putFloat("lux_close", lux_close_threshold);
  prefs.putFloat("lux_open", lux_open_threshold);
  prefs.putFloat("wind_retract", wind_retract_threshold);
  prefs.putFloat("temp_close", temp_close_threshold);
  prefs.putInt("shade_open_angle", shade_open_angle);
  prefs.putInt("shade_closed_angle", shade_closed_angle);
  prefs.putInt("shade_dwell_s", (int)(min_shade_dwell_ms / 1000UL));
}

void connectWiFiIfConfigured() {
  if (strlen(WIFI_SSID) == 0) return;
  Serial.print("Connecting to WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    wifiConnected = true;
    // optional: sync NTP if needed
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  } else {
    Serial.println("\nWiFi failed to connect");
  }
}

void postJsonToServer(const String& json) {
  if (!wifiConnected || strlen(SERVER_URL) == 0) return;
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  if (code > 0) {
    String payload = http.getString();
    Serial.printf("HTTP %d: %s\n", code, payload.c_str());
  } else {
    Serial.printf("HTTP POST failed, err=%d\n", code);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  Wire.begin(); // SDA=21, SCL=22 on most ESP32 boards
  bh1750Init();
  dht.begin();

  // direction pins
  for (int i = 0; i < 6; ++i) {
    pinMode(DIR_PINS[i], INPUT); // external pull-ups required on 34..39
  }

  // Hall sensor
  pinMode(HALL_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, RISING);

  // servo (LEDC)
  ledcSetup(SERVO_LEDC_CHANNEL, SERVO_LEDC_FREQ, SERVO_LEDC_RES);
  ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL);

  // Preferences & config
  loadConfig();

  // initial shade position
  setServoAngle(shade_open_angle);

  // optional WiFi
  connectWiFiIfConfigured();

  lastReportMs = millis();
  lastHallCountSnapshot = 0;
  Serial.println("Weather station startup complete");
}

void loop() {
  unsigned long nowMs = millis();
  // DHT read (slow; read every loop but data will internally manage timing)
  float tempC = dht.readTemperature();
  float hum = dht.readHumidity();

  // lux sample (non-blocking driver reads 2 bytes; BH1750 continuous mode returns latest)
  float lux = bh1750ReadLux();
  if (lux < 0) lux = NAN;

  if (nowMs - lastReportMs >= REPORT_INTERVAL_MS) {
    unsigned long pulsesNow;
    noInterrupts();
    pulsesNow = hallPulseCount;
    interrupts();

    unsigned long pulsesSince = pulsesNow - lastHallCountSnapshot;
    lastHallCountSnapshot = pulsesNow;

    float windowSec = REPORT_INTERVAL_MS / 1000.0f;
    float pulseRateHz = ((float)pulsesSince) / windowSec;
    float revsPerSec = pulseRateHz / (float)pulses_per_rev;
    float windMps = k_speed * revsPerSec;

    float windDirDeg = getDirectionDeg();

    // Shade control logic with hysteresis and minimum dwell
    bool moveShade = false;
    int targetAngle = current_shade_angle;
    // safety: retract if high wind
    if (windMps > wind_retract_threshold) {
      targetAngle = shade_open_angle;
    } else {
      // shade decision by lux and optional temp
      if (lux >= 0) {
        if ((lux > lux_close_threshold) && (!isnan(tempC) ? tempC > temp_close_threshold : true)) {
          targetAngle = shade_closed_angle;
        } else if (lux < lux_open_threshold) {
          targetAngle = shade_open_angle;
        }
      }
    }
    // enforce min dwell time
    if (targetAngle != current_shade_angle && (millis() - last_shade_move_ms) > min_shade_dwell_ms) {
      setServoAngle(targetAngle);
      moveShade = true;
    }

    // Build JSON payload
    unsigned long ts = nowMs / 1000UL; // seconds since boot (not epoch unless WiFi+NTP)
    String payload = "{";
    payload += "\"ts\":" + String(ts) + ",";
    payload += "\"temp_c\":" + (isnan(tempC) ? String("null") : String(tempC, 2)) + ",";
    payload += "\"rh_pct\":" + (isnan(hum) ? String("null") : String(hum, 2)) + ",";
    payload += "\"wind_mps\":" + String(windMps, 3) + ",";
    payload += "\"wind_deg\":" + String(windDirDeg, 1) + ",";
    payload += "\"lux\":" + (isnan(lux) ? String("null") : String((uint32_t)lux)) + ",";
    payload += "\"shade_angle\":" + String(current_shade_angle);
    payload += "}";

    Serial.println(payload); // primary transport for now

    // optional: HTTP POST to server
    if (wifiConnected && strlen(SERVER_URL) > 0) {
      postJsonToServer(payload);
    }

    lastReportMs += REPORT_INTERVAL_MS;
  }

  delay(200); // small delay to yield; adjust as needed
}
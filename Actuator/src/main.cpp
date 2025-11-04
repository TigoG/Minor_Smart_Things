#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "ShadeController.h"
#include "CommandProcessor.h"
#include "secret.h"
#include <stdlib.h>
#include <string.h>

// Default pin for MG90 servo
const int SERVO_PIN = 4;

// Default motion parameters
const float DEFAULT_ANGLE = 45.0f;
const unsigned long DEFAULT_UP_DURATION = 5000UL;   // 5s
const unsigned long DEFAULT_DOWN_DURATION = 10000UL; // 10s

// define the extern from the header
ShadeController *gShadeController = nullptr;

// Command processor instance (owns parsing logic)
CommandProcessor *gCommandProcessor = nullptr;

// MQTT + WiFi client for receiving sensor data
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
// TODO: set these to your WiFi credentials
static const char* WIFI_SSID = secret::WIFI_SSID;
static const char* WIFI_PASS = secret::WIFI_PASS;
static const char* MQTT_BROKER = secret::MQTT_BROKER;
static const uint16_t MQTT_PORT = secret::MQTT_PORT;
static const char* MQTT_TOPIC_BASE = secret::MQTT_TOPIC_BASE;

// Running copy of latest sensor values (shared SensorPayload struct from ShadeController.h)
static SensorPayload gLatestPayload;
static unsigned long lastMqttReconnectAttempt = 0;

// MQTT message callback: update the running sensor struct and hand it to the ShadeController
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String msg;
  if (length > 0) msg = String((char*)payload, length); else msg = "";
  msg.trim();
  int idx = t.lastIndexOf('/');
  String key = (idx >= 0) ? t.substring(idx + 1) : t;
  key.toLowerCase();

  bool isNull = msg.equalsIgnoreCase("null") || msg.equalsIgnoreCase("nan") || msg.length() == 0;

  if (key == "temperature") {
    if (isNull) gLatestPayload.tempC = NAN; else gLatestPayload.tempC = msg.toFloat();
  } else if (key == "humidity") {
    if (isNull) gLatestPayload.humidity = NAN; else gLatestPayload.humidity = msg.toFloat();
  } else if (key == "windspeed") {
    if (isNull) gLatestPayload.wind_kmh = NAN; else gLatestPayload.wind_kmh = msg.toFloat();
  } else if (key == "light") {
    if (isNull) gLatestPayload.lux = NAN; else gLatestPayload.lux = msg.toFloat();
  } else if (key == "seq") {
    if (isNull) gLatestPayload.seq = 0; else gLatestPayload.seq = (uint32_t)strtoul(msg.c_str(), NULL, 10);
  } else {
    // unknown topic suffix - ignore
  }

  Serial.printf("MQTT recv: %s = %s\n", topic, msg.c_str());

  if (gShadeController) {
    gShadeController->handleMessage((const uint8_t*)&gLatestPayload, sizeof(gLatestPayload));
  }
}

// Try to connect to MQTT broker and subscribe to sensor topics
void mqttReconnect() {
  if (mqttClient.connected()) return;
  if (millis() - lastMqttReconnectAttempt < 5000) return;
  lastMqttReconnectAttempt = millis();
  Serial.println("MQTT: attempting reconnect...");
  char clientId[48];
  String mac = WiFi.macAddress();
  String id = "actuator-";
  id += mac;
  id.toCharArray(clientId, sizeof(clientId));
  if (mqttClient.connect(clientId)) {
    Serial.println("MQTT connected");
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/Temperature", MQTT_TOPIC_BASE);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/Humidity", MQTT_TOPIC_BASE);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/Windspeed", MQTT_TOPIC_BASE);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/Light", MQTT_TOPIC_BASE);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/Seq", MQTT_TOPIC_BASE);
    mqttClient.subscribe(topic);
  } else {
    Serial.printf("MQTT connect failed, rc=%d\n", mqttClient.state());
  }
}

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  HELP");
  Serial.println("  SENSOR <tempC|NaN> <humidity|NaN> <lux|NaN> <wind_kmh|NaN> <seq>");
  Serial.println("  UP [angle] [duration_ms]");
  Serial.println("  DOWN [angle] [duration_ms]");
  Serial.println("  OPEN [angle_rel] [hold_ms]");
  Serial.println("  CLOSE [angle_rel] [hold_ms]");
  Serial.println("  PULSE <angle_rel> <hold_ms> [move_ms]");
  Serial.println("  STATUS");
}

void handleSensorCommand() {
  if (!gShadeController) { Serial.println("No ShadeController instance"); return; }
  float temp = NAN, hum = NAN, lux = NAN, wind = NAN;
  uint32_t seq = 0;
  char *tok = strtok(NULL, " \t"); if (tok) temp = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hum  = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) lux  = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) wind = (strcmp(tok, "NaN") == 0 || strcmp(tok, "nan") == 0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) seq = (uint32_t)strtoul(tok, NULL, 10);

  SensorPayload payload;
  payload.tempC = temp;
  payload.humidity = hum;
  payload.lux = lux;
  payload.wind_kmh = wind;
  payload.seq = seq;

  Serial.printf("Injecting sensor: temp=%0.1f hum=%0.1f lux=%0.1f wind=%0.2f seq=%u\n",
                payload.tempC, payload.humidity, payload.lux, payload.wind_kmh, payload.seq);

  gShadeController->handleMessage((const uint8_t *)&payload, sizeof(payload));
}

void handleUpDownCommand(bool isUp) {
  float angle = DEFAULT_ANGLE;
  unsigned long duration = isUp ? DEFAULT_UP_DURATION : DEFAULT_DOWN_DURATION;
  char *tok = strtok(NULL, " \t"); if (tok) angle = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) duration = (unsigned long)strtoul(tok, NULL, 10);

  String payload = isUp ? "up" : "down";
  payload += " angle:";
  payload += String(angle, 1);
  payload += " duration:";
  payload += String(duration);

  Serial.printf("Injecting command: %s\n", payload.c_str());
  if (gShadeController) gShadeController->handleMessage((const uint8_t *)payload.c_str(), payload.length());
}

void handleOpenCloseCommand(bool isOpen) {
  if (!gShadeController) { Serial.println("No ShadeController instance"); return; }
  float angle_rel = DEFAULT_ANGLE; // relative to baseline
  unsigned long hold = DEFAULT_UP_DURATION;
  char *tok = strtok(NULL, " \t"); if (tok) angle_rel = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hold = (unsigned long)strtoul(tok, NULL, 10);

  if (isOpen) {
    Serial.printf("OPEN: pulse +%0.1f for %lu ms\n", angle_rel, hold);
    gShadeController->pulseAngle(angle_rel, hold);
  } else {
    Serial.printf("CLOSE: pulse -%0.1f for %lu ms\n", angle_rel, hold);
    gShadeController->pulseAngle(-angle_rel, hold);
  }
}

void handlePulseCommand() {
  if (!gShadeController) { Serial.println("No ShadeController instance"); return; }
  float angle_rel = 0.0f;
  unsigned long hold = DEFAULT_UP_DURATION;
  unsigned long move_ms = 500UL;
  char *tok = strtok(NULL, " \t"); if (tok) angle_rel = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hold = (unsigned long)strtoul(tok, NULL, 10);
  tok = strtok(NULL, " \t"); if (tok) move_ms = (unsigned long)strtoul(tok, NULL, 10);

  Serial.printf("PULSE: angle=%0.1f hold=%lu move=%lu\n", angle_rel, hold, move_ms);
  gShadeController->pulseAngle(angle_rel, hold, move_ms);
}

void processSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  char buf[256];
  line.toCharArray(buf, sizeof(buf));
  char *tok = strtok(buf, " \t");
  if (!tok) return;
  String cmd = String(tok);
  cmd.toUpperCase();

  if (cmd == "HELP") { printHelp(); return; }
  if (cmd == "SENSOR") { handleSensorCommand(); return; }
  if (cmd == "UP") { handleUpDownCommand(true); return; }
  if (cmd == "DOWN") { handleUpDownCommand(false); return; }
  if (cmd == "OPEN") { handleOpenCloseCommand(true); return; }
  if (cmd == "CLOSE") { handleOpenCloseCommand(false); return; }
  if (cmd == "PULSE") { handlePulseCommand(); return; }
  if (cmd == "STATUS") { Serial.println("ShadeController configured."); return; }

  Serial.printf("Unknown command: %s\n", cmd.c_str());
  printHelp();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // create controller instance with servo pin
  gShadeController = new ShadeController(SERVO_PIN, DEFAULT_ANGLE, DEFAULT_UP_DURATION, DEFAULT_DOWN_DURATION);
  gShadeController->begin();

  // create command processor and hand it the controller
  gCommandProcessor = new CommandProcessor(gShadeController, DEFAULT_ANGLE, DEFAULT_UP_DURATION, DEFAULT_DOWN_DURATION);

  // Initialize MQTT client and WiFi to subscribe to sensor topics
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
 
  Serial.printf("Connecting to WiFi '%s' ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected (will retry in loop)");
  }
 
  // initialize latest payload to NaN
  gLatestPayload.tempC = NAN;
  gLatestPayload.humidity = NAN;
  gLatestPayload.lux = NAN;
  gLatestPayload.wind_kmh = NAN;
  gLatestPayload.seq = 0;
 
  mqttReconnect();

  Serial.println("Ready. Type HELP for commands.");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    if (gCommandProcessor) gCommandProcessor->processLine(line);
  }

  // Maintain MQTT connection & process incoming messages
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      mqttReconnect();
    } else {
      mqttClient.loop();
    }
  } else {
    // Attempt to reconnect WiFi periodically
    static unsigned long lastWifiAttempt = 0;
    if (millis() - lastWifiAttempt > 5000) {
      lastWifiAttempt = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  delay(10);
}

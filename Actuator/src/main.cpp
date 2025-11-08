#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "ShadeController.h"
#include "CommandProcessor.h"
#include "secret.h"
#include <stdlib.h>
#include <string.h>

// Default pin for MG90 servo
const int SERVO_PIN = 4;

// Default motion parameters
const float DEFAULT_ANGLE = 90.0f;
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
static const char* MQTT_USER = secret::MQTT_USER;
static const char* MQTT_PASS = secret::MQTT_PASS;
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

  // Special-case: motor topic toggles shade open/close when payload == "1"
  if (t.equalsIgnoreCase("homestations/1051804/0/motor") || t.endsWith("/motor")) {
    Serial.printf("MQTT motor topic: %s -> %s\n", topic, msg.c_str());
    if (msg.equals("1")) {
      if (gShadeController) {
        if (gShadeController->isOpen()) {
          Serial.println("MQTT: motor=1 -> currently OPEN, sending CLOSE");
          String cmd = "close";
          gShadeController->handleMessage((const uint8_t*)cmd.c_str(), cmd.length());
        } else {
          Serial.println("MQTT: motor=1 -> currently CLOSED/UNKNOWN, sending OPEN");
          String cmd = "open";
          gShadeController->handleMessage((const uint8_t*)cmd.c_str(), cmd.length());
        }
      }
    } else {
      Serial.printf("MQTT motor: payload '%s' ignored\n", msg.c_str());
    }
    return;
  }

  bool isNull = msg.equalsIgnoreCase("null") || msg.equalsIgnoreCase("nan") || msg.length() == 0;

  if (key == "temperature") {
    if (isNull) gLatestPayload.tempC = NAN; else gLatestPayload.tempC = msg.toFloat();
  } else if (key == "humidity") {
    if (isNull) gLatestPayload.humidity = NAN; else gLatestPayload.humidity = msg.toFloat();
  } else if (key == "windspeed") {
    if (isNull) gLatestPayload.wind_kmh = NAN; else gLatestPayload.wind_kmh = msg.toFloat();
  } else if (key == "light") {
    if (isNull) gLatestPayload.lux = NAN; else gLatestPayload.lux = msg.toFloat();
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
  if (mqttClient.connect(clientId, MQTT_USER, MQTT_PASS)) {
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
    // Subscribe to motor control topic used externally
    mqttClient.subscribe("homestations/1051804/0/motor");
  } else {
    Serial.printf("MQTT connect failed, rc=%d\n", mqttClient.state());
  }
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

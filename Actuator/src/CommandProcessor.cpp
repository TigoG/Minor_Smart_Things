#include "CommandProcessor.h"
#include <stdlib.h>
#include <string.h>

CommandProcessor::CommandProcessor(ShadeController* controller,
                                   float defaultAngle,
                                   unsigned long upDuration,
                                   unsigned long downDuration)
  : _controller(controller),
    _defaultAngle(defaultAngle),
    _upDuration(upDuration),
    _downDuration(downDuration) {}

CommandProcessor::~CommandProcessor() {}

void CommandProcessor::printHelp() {
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

void CommandProcessor::handleSensorCommand() {
  if (!_controller) { Serial.println("No ShadeController instance"); return; }
  float temp = NAN, hum = NAN, lux = NAN, wind = NAN;
  uint32_t seq = 0;
  char* tok = strtok(NULL, " \t"); if (tok) temp = (strcmp(tok, "NaN")==0 || strcmp(tok, "nan")==0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hum = (strcmp(tok, "NaN")==0 || strcmp(tok, "nan")==0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) lux = (strcmp(tok, "NaN")==0 || strcmp(tok, "nan")==0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) wind = (strcmp(tok, "NaN")==0 || strcmp(tok, "nan")==0) ? NAN : atof(tok);
  tok = strtok(NULL, " \t"); if (tok) seq = (uint32_t)strtoul(tok, NULL, 10);

  SensorPayload payload;
  payload.tempC = temp;
  payload.humidity = hum;
  payload.lux = lux;
  payload.wind_kmh = wind;
  payload.seq = seq;

  Serial.printf("Injecting sensor: temp=%0.1f hum=%0.1f lux=%0.1f wind=%0.2f seq=%u\n",
                payload.tempC, payload.humidity, payload.lux, payload.wind_kmh, payload.seq);

  _controller->handleMessage((const uint8_t *)&payload, sizeof(payload));
}

void CommandProcessor::handleUpDownCommand(bool isUp) {
  float angle = _defaultAngle;
  unsigned long duration = isUp ? _upDuration : _downDuration;
  char* tok = strtok(NULL, " \t"); if (tok) angle = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) duration = (unsigned long)strtoul(tok, NULL, 10);

  String payload = isUp ? "up" : "down";
  payload += " angle:";
  payload += String(angle, 1);
  payload += " duration:";
  payload += String(duration);

  Serial.printf("Injecting command: %s\n", payload.c_str());
  if (_controller) _controller->handleMessage((const uint8_t *)payload.c_str(), payload.length());
}

void CommandProcessor::handleOpenCloseCommand(bool isOpen) {
  if (!_controller) { Serial.println("No ShadeController instance"); return; }
  float angle_rel = _defaultAngle;
  unsigned long hold = _upDuration;
  char* tok = strtok(NULL, " \t"); if (tok) angle_rel = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hold = (unsigned long)strtoul(tok, NULL, 10);

  if (isOpen) {
    Serial.printf("OPEN: pulse +%0.1f for %lu ms\n", angle_rel, hold);
    _controller->pulseAngle(angle_rel, hold);
  } else {
    Serial.printf("CLOSE: pulse -%0.1f for %lu ms\n", angle_rel, hold);
    _controller->pulseAngle(-angle_rel, hold);
  }
}

void CommandProcessor::handlePulseCommand() {
  if (!_controller) { Serial.println("No ShadeController instance"); return; }
  float angle_rel = 0.0f;
  unsigned long hold = _upDuration;
  unsigned long move_ms = 500UL;
  char* tok = strtok(NULL, " \t"); if (tok) angle_rel = atof(tok);
  tok = strtok(NULL, " \t"); if (tok) hold = (unsigned long)strtoul(tok, NULL, 10);
  tok = strtok(NULL, " \t"); if (tok) move_ms = (unsigned long)strtoul(tok, NULL, 10);

  Serial.printf("PULSE: angle=%0.1f hold=%lu move=%lu\n", angle_rel, hold, move_ms);
  _controller->pulseAngle(angle_rel, hold, move_ms);
}

void CommandProcessor::processLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  char buf[256];
  line.toCharArray(buf, sizeof(buf));
  char* tok = strtok(buf, " \t");
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
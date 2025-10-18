#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>
#include "ShadeController.h"

class CommandProcessor {
public:
  CommandProcessor(ShadeController* controller,
                   float defaultAngle = 45.0f,
                   unsigned long upDuration = 5000UL,
                   unsigned long downDuration = 10000UL);
  ~CommandProcessor();

  // Parse a single line from Serial and dispatch commands
  void processLine(String line);

  // Print help text to Serial
  void printHelp();

private:
  ShadeController* _controller;
  float _defaultAngle;
  unsigned long _upDuration;
  unsigned long _downDuration;

  void handleSensorCommand();
  void handleUpDownCommand(bool isUp);
  void handleOpenCloseCommand(bool isOpen);
  void handlePulseCommand();
};

#endif // COMMAND_PROCESSOR_H
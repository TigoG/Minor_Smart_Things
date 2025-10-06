#ifndef STEPPER_CONTROLLER_H
#define STEPPER_CONTROLLER_H

#include <Arduino.h>

class StepperController {
public:
  StepperController(int stepPin, int dirPin, int enablePin = -1,
                    int stepsPerRev = 200, int microstep = 16,
                    float gearRatio = 1.0f, unsigned int pulseWidthUs = 5,
                    bool simulate = false);

  void begin();
  void enable();
  void disable();

  long stepsForAngle(float angleDeg) const;

  // steps: number of steps to move; stepDelayUs: microseconds between step pulses;
  // dirRight: true = right/high, false = left/low
  void step(long steps, unsigned long stepDelayUs, bool dirRight);

  // high level: move by angle over duration in ms, direction right/left
  void moveAngle(float angleDeg, unsigned long durationMs, bool dirRight);

  // Simulation control
  bool isSimulating() const;
  void setSimulate(bool simulate);

private:
  int _stepPin;
  int _dirPin;
  int _enablePin;
  int _stepsPerRev;
  int _microstep;
  float _gearRatio;
  unsigned int _pulseWidthUs;
  volatile bool _isMoving;
  bool _simulate;
};

#endif // STEPPER_CONTROLLER_H
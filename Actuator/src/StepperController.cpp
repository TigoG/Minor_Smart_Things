#include "StepperController.h"

StepperController::StepperController(int stepPin, int dirPin, int enablePin,
                                    int stepsPerRev, int microstep,
                                    float gearRatio, unsigned int pulseWidthUs,
                                    bool simulate)
    : _stepPin(stepPin),
      _dirPin(dirPin),
      _enablePin(enablePin),
      _stepsPerRev(stepsPerRev),
      _microstep(microstep),
      _gearRatio(gearRatio),
      _pulseWidthUs(pulseWidthUs),
      _isMoving(false),
      _simulate(simulate) {}

void StepperController::begin() {
  if (!_simulate) {
    pinMode(_stepPin, OUTPUT);
    pinMode(_dirPin, OUTPUT);
    if (_enablePin >= 0) pinMode(_enablePin, OUTPUT);
    disable(); // keep driver disabled by default
  } else {
    Serial.println("StepperController (SIM): begin");
  }
}

void StepperController::enable() {
  if (_simulate) {
    Serial.println("StepperController (SIM): enable driver (no GPIO changes)");
    return;
  }
  if (_enablePin >= 0) digitalWrite(_enablePin, LOW); // active low for A4988
}

void StepperController::disable() {
  if (_simulate) {
    Serial.println("StepperController (SIM): disable driver (no GPIO changes)");
    return;
  }
  if (_enablePin >= 0) digitalWrite(_enablePin, HIGH);
}

long StepperController::stepsForAngle(float angleDeg) const {
  double steps = (angleDeg / 360.0) * _stepsPerRev * _microstep * _gearRatio;
  return (long)round(steps);
}

void StepperController::step(long steps, unsigned long stepDelayUs, bool dirRight) {
  if (steps <= 0) return;
  if (_isMoving) {
    Serial.println("Stepper busy - ignoring new command");
    return;
  }
  _isMoving = true;

  if (!_simulate) {
    digitalWrite(_dirPin, dirRight ? HIGH : LOW);
    enable();

    for (long i = 0; i < steps; ++i) {
      digitalWrite(_stepPin, HIGH);
      delayMicroseconds(_pulseWidthUs);
      digitalWrite(_stepPin, LOW);

      if (stepDelayUs > (unsigned long)_pulseWidthUs) {
        unsigned long remainingUs = stepDelayUs - (unsigned long)_pulseWidthUs;
        if (remainingUs >= 1000) {
          delay(remainingUs / 1000);
          unsigned long rem = remainingUs % 1000;
          if (rem) delayMicroseconds(rem);
        } else {
          delayMicroseconds(remainingUs);
        }
      }

      yield(); // allow background tasks (WiFi/ESP-NOW)
    }

    disable();
    _isMoving = false;
    return;
  }

  // Simulation mode: log steps to Serial and respect timing so durations match real runs
  Serial.printf("SIM: stepping %ld steps (%s) stepDelay=%luus pulse=%u us\n",
                steps, dirRight ? "RIGHT" : "LEFT", stepDelayUs, _pulseWidthUs);

  // Print summary lines but avoid flooding by limiting per-step prints for large step counts
  const long maxPrintSteps = 50;
  long printInterval = (steps <= maxPrintSteps) ? 1 : (steps / maxPrintSteps);

  for (long i = 0; i < steps; ++i) {
    if ((i % printInterval) == 0) {
      Serial.printf("SIM: step %ld/%ld\n", i + 1, steps);
    }

    // emulate pulse + delay timing
    if (stepDelayUs > (unsigned long)_pulseWidthUs) {
      unsigned long remainingUs = stepDelayUs - (unsigned long)_pulseWidthUs;
      if (remainingUs >= 1000) {
        delay(remainingUs / 1000);
        unsigned long rem = remainingUs % 1000;
        if (rem) delayMicroseconds(rem);
      } else {
        delayMicroseconds(remainingUs);
      }
    } else if (stepDelayUs > 0) {
      delayMicroseconds(stepDelayUs);
    }

    yield();
  }

  Serial.println("SIM: stepping complete");
  _isMoving = false;
}

void StepperController::moveAngle(float angleDeg, unsigned long durationMs, bool dirRight) {
  long steps = stepsForAngle(angleDeg);
  if (steps <= 0) {
    Serial.println("Calculated 0 steps for angle - nothing to do");
    return;
  }
  unsigned long totalUs = durationMs * 1000UL;
  unsigned long stepDelayUs = totalUs / (unsigned long)steps;
  Serial.printf("%s: move %ld steps (%.1f deg) %s over %lums -> %lu us step delay\n",
                _simulate ? "SIM" : "Stepper", steps, angleDeg, dirRight ? "right" : "left", durationMs, stepDelayUs);
  step(steps, stepDelayUs, dirRight);
}

bool StepperController::isSimulating() const {
  return _simulate;
}

void StepperController::setSimulate(bool simulate) {
  _simulate = simulate;
  Serial.printf("StepperController: setSimulate -> %s\n", _simulate ? "true" : "false");
}
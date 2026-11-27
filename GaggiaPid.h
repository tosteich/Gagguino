#pragma once
#include <Arduino.h>
#include <math.h>

// Preserve the original Gagguino PID mathematics and fixed update cadence.
// Compatibility methods are retained for application safety-state transitions.
class GaggiaPid {
private:
  float iState = 0;
  float lastPoint = 0;
  bool skipDerivativeOnce = false;
  float pTerm = 0;
  float dTerm = 0;
  float Kp = 0, Ki = 0, Kd = 0;
  float setPoint = 0;
  float periodSec;
  const int maxPower = 100;

public:
  explicit GaggiaPid(int period) : periodSec(period / 1000.0f) {}

  void setupPid(float Kp, float Ki, float Kd, float targetPoint) {
    this->Kp = Kp;
    this->Ki = Ki;
    this->Kd = Kd;
    this->setPoint = targetPoint;
  }

  void setKp(float value) { Kp = value; }
  void setKi(float value) { Ki = value; }
  void setKd(float value) { Kd = value; }
  float getKp() { return Kp; }
  float getKi() { return Ki; }
  float getKd() { return Kd; }
  void setSetPoint(float value) { setPoint = value; }
  float getSetPoint() { return setPoint; }

  // Read-only diagnostic accessors. No change to pump or boiler mathematics.
  float getPTerm() const { return pTerm; }
  float getITerm() const { return iState; }
  float getDTerm() const { return dTerm; }

  // Retain explicit reset used by sensor-fault protection and mode changes.
  void resetState() {
    iState = 0;
    lastPoint = 0;
    skipDerivativeOnce = true;
    pTerm = 0;
    dTerm = 0;
  }

  // Legacy algorithm, including historical D sign and integral limits.
  int getResult(float input) {
    if (periodSec <= 0 || !isfinite(input) || !isfinite(setPoint) ||
        !isfinite(Kp) || !isfinite(Ki) || !isfinite(Kd)) {
      return 0;
    }

    float error = setPoint - input;
    pTerm = Kp * error;
    dTerm = skipDerivativeOnce ? 0.0f : Kd * (input - lastPoint) / periodSec;
    skipDerivativeOnce = false;
    lastPoint = input;
    iState += error * Ki * periodSec;

    float result = constrain((pTerm + dTerm), 0, maxPower);
    if (Ki != 0) {
      iState = constrain(iState, (0 - result) / (Ki * periodSec),
                         (maxPower - result) / (Ki * periodSec));
    }
    iState = constrain(iState, 0, maxPower);
    result = (int)constrain((result + iState), 0, maxPower);
    return result;
  }
};

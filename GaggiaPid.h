#pragma once
#include <Arduino.h>

class GaggiaPid {
private:
  float iState = 0;
  float lastPoint = 0;
  bool hasLastPoint = false;
  float Kp = 0, Ki = 0, Kd = 0;
  float setPoint = 0;
  const float periodSec;
  const int maxPower = 100;

public:
  explicit GaggiaPid(int period) : periodSec(period > 0 ? period / 1000.0f : 0.0f) {}

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

  int getResult(float input) {
    if (periodSec <= 0.0f || !isfinite(input)) {
      return 0;
    }

    const float error = setPoint - input;
    const float pTerm = Kp * error;

    // Preserve the historical derivative direction until hardware testing.
    const float dTerm = hasLastPoint ? Kd * (input - lastPoint) / periodSec : 0.0f;
    lastPoint = input;
    hasLastPoint = true;

    // iState is already the integral contribution in output units.
    const float deltaI = error * Ki * periodSec;
    const float proposedI = constrain(iState + deltaI, -(float)maxPower, (float)maxPower);
    const float proposedOutput = pTerm + dTerm + proposedI;

    // Clamp integration at the saturation boundary without building windup.
    if (proposedOutput > maxPower && deltaI > 0.0f) {
      const float limit = constrain((float)maxPower - pTerm - dTerm,
                                    -(float)maxPower, (float)maxPower);
      iState = max(iState, limit);
    } else if (proposedOutput < 0.0f && deltaI < 0.0f) {
      const float limit = constrain(-pTerm - dTerm, -(float)maxPower, (float)maxPower);
      iState = min(iState, limit);
    } else {
      iState = proposedI;
    }

    return (int)constrain(pTerm + dTerm + iState, 0.0f, (float)maxPower);
  }
};

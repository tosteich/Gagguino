#pragma once
#include <Arduino.h>

class GaggiaPid {
private:
  float iState = 0;
  float lastPoint = 0;
  bool hasLastPoint = false;
  float filteredDerivative = 0.0f;
  float derivativeAlpha = 1.0f; // 1.0 preserves unfiltered behavior.
  bool derivativeOpposesMeasurement = false; // legacy pump behavior is default
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
  // Alpha in (0, 1]: smaller values reduce measurement noise in D.
  // Defaults to 1 so existing tuning is unchanged.
  void setDerivativeFilterAlpha(float alpha) {
    if (!isfinite(alpha) || alpha <= 0.0f || alpha > 1.0f) {
      return;
    }
    derivativeAlpha = alpha;
    filteredDerivative = 0.0f;
    hasLastPoint = false;
  }

  // Use conventional negative derivative-on-measurement for the boiler.
  // Default false preserves the pump's historical positive derivative.
  void setDerivativeOpposesMeasurement(bool enabled) {
    if (derivativeOpposesMeasurement != enabled) {
      derivativeOpposesMeasurement = enabled;
      resetState();
    }
  }

  // An explicit reset for mode transitions; not invoked automatically.
  void resetState() {
    iState = 0.0f;
    filteredDerivative = 0.0f;
    hasLastPoint = false;
  }

  void setSetPoint(float value) { setPoint = value; }
  float getSetPoint() { return setPoint; }

  int getResult(float input) {
    if (periodSec <= 0.0f || !isfinite(input) ||
        !isfinite(setPoint) || !isfinite(Kp) || !isfinite(Ki) || !isfinite(Kd)) {
      // Do not bridge an invalid-sample gap when estimating derivative.
      // Retain the integral contribution; only the D measurement history is stale.
      hasLastPoint = false;
      filteredDerivative = 0.0f;
      return 0;
    }

    const float error = setPoint - input;
    const float pTerm = Kp * error;

    // Derivative is taken on measurement (avoids setpoint kick).
    // Boiler opposes rising temperature; pump retains legacy direction.
    const float measurementRate =
        hasLastPoint ? (input - lastPoint) / periodSec : 0.0f;
    filteredDerivative = hasLastPoint
        ? derivativeAlpha * measurementRate + (1.0f - derivativeAlpha) * filteredDerivative
        : 0.0f;
    const float dTerm = (derivativeOpposesMeasurement ? -Kd : Kd) * filteredDerivative;
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

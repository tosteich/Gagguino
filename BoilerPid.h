#pragma once
#include <Arduino.h>
#include <math.h>

// EXPERIMENTAL boiler-only PID. Not used in the normal firmware build.
// Output remains 0..100%; sampling cadence is provided by the caller.
// Derivative is taken on the measurement with a negative sign to avoid
// setpoint kick. Conditional integration avoids windup against limits.
// Do not enable on a mains-powered boiler before independent bench tests.
class BoilerPid {
private:
  float integral = 0.0f;
  float lastP_ = 0.0f;
  float lastD_ = 0.0f;
  float lastMeasurement = 0.0f;
  bool haveLastMeasurement = false;
  float kp = 0.0f, ki = 0.0f, kd = 0.0f;
  float setPoint = 0.0f;
  const float periodSec;
  static constexpr float maxPower = 100.0f;

  static float clampPower(float value) {
    return value < 0.0f ? 0.0f : (value > maxPower ? maxPower : value);
  }

public:
  explicit BoilerPid(int periodMs) : periodSec(periodMs / 1000.0f) {}

  void setupPid(float Kp, float Ki, float Kd, float targetPoint) {
    kp = Kp;
    ki = Ki;
    kd = Kd;
    setPoint = targetPoint;
    resetState();
  }

  void setKp(float value) { kp = value; }
  void setKi(float value) { ki = value; }
  void setKd(float value) { kd = value; }
  float getKp() const { return kp; }
  float getKi() const { return ki; }
  float getKd() const { return kd; }
  void setSetPoint(float value) { setPoint = value; }
  float getSetPoint() const { return setPoint; }
  float getPTerm() const { return lastP_; }
  float getITerm() const { return integral; }
  float getDTerm() const { return lastD_; }

  void resetState() {
    integral = 0.0f;
    lastP_ = 0.0f;
    lastD_ = 0.0f;
    lastMeasurement = 0.0f;
    haveLastMeasurement = false;
  }

  int getResult(float measurement) {
    if (periodSec <= 0.0f || !isfinite(measurement) ||
        !isfinite(setPoint) || !isfinite(kp) || !isfinite(ki) ||
        !isfinite(kd) || kp < 0.0f || ki < 0.0f || kd < 0.0f) {
      resetState();
      return 0;
    }

    const float error = setPoint - measurement;
    const float proportional = kp * error;
    const float derivative = haveLastMeasurement
        ? -kd * (measurement - lastMeasurement) / periodSec
        : 0.0f;
    lastMeasurement = measurement;
    haveLastMeasurement = true;

    lastP_ = proportional;
    lastD_ = derivative;
    const float withoutNewIntegral = proportional + derivative + integral;
    const float proposedIntegral = integral + ki * periodSec * error;
    // Do not accumulate while already saturated in the same direction;
    // allow integral to unwind when error points back into the output range.
    if (!((withoutNewIntegral >= maxPower && error > 0.0f) ||
          (withoutNewIntegral <= 0.0f && error < 0.0f))) {
      integral = clampPower(proposedIntegral);
    }
    return static_cast<int>(clampPower(proportional + derivative + integral));
  }
};

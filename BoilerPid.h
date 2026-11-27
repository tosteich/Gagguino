#pragma once
#include <Arduino.h>
#include <math.h>

// EXPERIMENTAL boiler PID, currently used only for shadow diagnostics.
// The active boiler controller and the pump still use the unchanged GaggiaPid.
// Do NOT enable this as the actuator controller without separate hardware testing.
class BoilerPid {
private:
  float integral = 0.0f;
  float lastP_ = 0.0f;
  float lastD_ = 0.0f;
  float lastMeasurement_ = 0.0f;
  float filteredRate_ = 0.0f;
  bool haveLastMeasurement_ = false;
  float kp = 0.0f, ki = 0.0f, kd = 0.0f;
  float setPoint = 0.0f;
  const float periodSec;

  static constexpr float maxPower = 100.0f;
  // Initial diagnostic-only parameters; NOT validated boiler tuning.
  static constexpr float maxIntegral = 20.0f;
  static constexpr float derivativeFilterSeconds = 1.5f;
  static constexpr float maxDerivativePower = 30.0f;
  static constexpr float aboveTargetUnwindSeconds = 5.0f;

  static float clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
  }

public:
  explicit BoilerPid(int periodMs) : periodSec(periodMs / 1000.0f) {}

  void setupPid(float Kp, float Ki, float Kd, float targetPoint) {
    kp = Kp; ki = Ki; kd = Kd; setPoint = targetPoint;
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
    lastMeasurement_ = 0.0f;
    filteredRate_ = 0.0f;
    haveLastMeasurement_ = false;
  }

  int getResult(float measurement) {
    if (periodSec <= 0.0f || !isfinite(measurement) ||
        !isfinite(setPoint) || !isfinite(kp) || !isfinite(ki) ||
        !isfinite(kd) || kp < 0.0f || ki < 0.0f || kd < 0.0f) {
      resetState();
      return 0;
    }

    const float error = setPoint - measurement;
    lastP_ = kp * error;

    // Derivative on measured temperature, low-pass filtered to suppress
    // LMT01 quantization spikes. A setpoint change cannot kick this term.
    if (haveLastMeasurement_) {
      const float rawRate = (measurement - lastMeasurement_) / periodSec;
      const float alpha = periodSec / (derivativeFilterSeconds + periodSec);
      filteredRate_ += alpha * (rawRate - filteredRate_);
    }
    lastMeasurement_ = measurement;
    haveLastMeasurement_ = true;
    lastD_ = clamp(-kd * filteredRate_, -maxDerivativePower, maxDerivativePower);

    const float withoutNewIntegral = lastP_ + lastD_ + integral;
    if (error < 0.0f) {
      // Do not let old positive integral sustain prolonged heating
      // above target. Exponential bleed plus ordinary negative error.
      const float bleed = clamp(periodSec / aboveTargetUnwindSeconds, 0.0f, 1.0f);
      integral *= 1.0f - bleed;
    }
    const float delta = ki * periodSec * error;
    // Conditional integration while saturated; always permit unwinding.
    if (!((withoutNewIntegral >= maxPower && delta > 0.0f) ||
          (withoutNewIntegral <= 0.0f && delta < 0.0f))) {
      integral = clamp(integral + delta, 0.0f, maxIntegral);
    }
    return static_cast<int>(clamp(lastP_ + integral + lastD_, 0.0f, maxPower));
  }
};

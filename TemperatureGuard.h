#pragma once
#include <stdint.h>

// Software plausibility gate for pulse-counted LMT01 temperature readings.
// Not a substitute for an independent boiler thermal cutoff.
class TemperatureGuard {
 public:
  static constexpr int kMinPulses = 800;     // 0 C
  static constexpr int kMaxPulses = 3104;    // 144 C
  static constexpr uint32_t kMaxAgeMs = 1000;
  static constexpr int kMaxStepPulses = 128; // 8 C between consecutive frames

  bool accept(int pulses, uint32_t nowMs) {
    if (pulses < kMinPulses || pulses > kMaxPulses) {
      reset();
      return false;
    }
    // After a timeout, requalification starts with a fresh pair.
    if (qualified_ && (uint32_t)(nowMs - lastMs_) > kMaxAgeMs) {
      reset();
    }
    if (!haveCandidate_) {
      candidate_ = pulses;
      haveCandidate_ = true;
      return false;
    }
    const int change = pulses > candidate_ ? pulses - candidate_ : candidate_ - pulses;
    if (change > kMaxStepPulses) {
      reset();
      candidate_ = pulses;
      haveCandidate_ = true;
      return false;
    }
    candidate_ = pulses;
    lastMs_ = nowMs;
    qualified_ = true;
    lastPulses_ = pulses;
    return true;
  }

  bool fresh(uint32_t nowMs) const {
    return qualified_ && (uint32_t)(nowMs - lastMs_) <= kMaxAgeMs;
  }

  float temperatureC() const { return lastPulses_ / 16.0f - 50.0f; }
  void reset() {
    haveCandidate_ = false;
    qualified_ = false;
    candidate_ = 0;
  }

 private:
  int candidate_ = 0;
  int lastPulses_ = 0;
  uint32_t lastMs_ = 0;
  bool haveCandidate_ = false;
  bool qualified_ = false;
};

#pragma once
#include <stdint.h>

// Logic-only plan. Not wired to GPIO or an ESP32 timer.
// This models exactly one gate pulse per detected zero-crossing event.
class PumpPhasePlan {
 public:
  static constexpr uint32_t kMinTicks = 1150;
  static constexpr uint32_t kMaxTicks = 49000;

  uint32_t zeroCross(int requestedPower) {
    if (requestedPower < 0) requestedPower = 0;
    if (requestedPower > 100) requestedPower = 100;
    delayTicks_ = static_cast<uint32_t>(49000 - 478.5 * requestedPower);
    pending_ = true;
    return delayTicks_;
  }

  bool takePulse() {
    if (!pending_) return false;
    pending_ = false;
    return true;
  }

  void cancel() { pending_ = false; }
  bool pending() const { return pending_; }
 private:
  uint32_t delayTicks_ = kMaxTicks;
  bool pending_ = false;
};

#pragma once
#include <math.h>

// Software backup limit for the experimental boiler PID.
// Independent hardware thermal cut-out is still mandatory.
// Latches until controller restart to avoid repeated heat cycling after trip.
class BoilerOvertempLatch {
  bool tripped_ = false;
 public:
  static constexpr float kMaxAboveTargetC = 5.0f;
  static constexpr float kAbsoluteMaxC = 140.0f;
  bool observe(float temperatureC, float targetC) {
    if (!isfinite(temperatureC) || !isfinite(targetC) ||
        targetC <= 0.0f || targetC > 140.0f ||
        temperatureC >= kAbsoluteMaxC ||
        temperatureC >= targetC + kMaxAboveTargetC) {
      tripped_ = true;
    }
    return tripped_;
  }
  bool tripped() const { return tripped_; }
};

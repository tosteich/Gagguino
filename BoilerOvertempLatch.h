#pragma once
#include <math.h>

// Boiler-only software backup overtemperature protection.
// Independent hardware thermal cut-out is still mandatory.
// Faults latch until reboot. A *normal* steam->brew transition is not a fault:
// while the boiler cools from the steam target, force heat OFF and postpone
// the brew-target relative limit until temperature is below that limit.
class BoilerOvertempLatch {
  bool tripped_ = false;
  bool steamCooldown_ = false;
 public:
  static constexpr float kMaxAboveTargetC = 5.0f;
  static constexpr float kAbsoluteMaxC = 140.0f;

  // Call ONLY on a confirmed steam->brew mode transition. This does not
  // reset any existing overtemperature latch.
  void beginSteamCooldown(float previousTargetC, float nextTargetC) {
    if (!isfinite(previousTargetC) || !isfinite(nextTargetC) ||
        previousTargetC <= nextTargetC || nextTargetC <= 0.0f ||
        nextTargetC > 140.0f) {
      steamCooldown_ = false;
      return;
    }
    steamCooldown_ = true;
  }

  // Entering steam mode ends the brew cooldown exemption. Does not reset
  // existing faults, which must remain latched.
  void cancelSteamCooldown() { steamCooldown_ = false; }

  bool observe(float temperatureC, float targetC) {
    if (tripped_) return true;
    if (!isfinite(temperatureC) || !isfinite(targetC) ||
        targetC <= 0.0f || targetC > 140.0f ||
        temperatureC >= kAbsoluteMaxC) {
      tripped_ = true;
      return true;
    }

    if (steamCooldown_) {
      // Suppress heating for the entire expected high-temperature cooldown.
      // Re-arm the relative brew safety limit once below its threshold.
      if (temperatureC < targetC + kMaxAboveTargetC) {
        steamCooldown_ = false;
      } else {
        return false;  // normal hot cooldown, not a latched fault
      }
    }
    if (temperatureC >= targetC + kMaxAboveTargetC) tripped_ = true;
    return tripped_;
  }

  bool heatInhibited() const { return tripped_ || steamCooldown_; }
  bool coolingAfterSteam() const { return steamCooldown_; }
  bool tripped() const { return tripped_; }
};

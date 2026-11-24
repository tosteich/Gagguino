#pragma once
#include <stdint.h>

// Preserves the original behavior: press is immediate; release requires
// more than 50 ms without observing the button held down.
class FlowButtonState {
public:
  explicit FlowButtonState(uint32_t releaseDelayMs = 50)
      : releaseDelayMs_(releaseDelayMs) {}

  bool update(bool isPressed, uint32_t nowMs) {
    if (isPressed) {
      pressed_ = true;
      lastPressedMs_ = nowMs;
    } else if (pressed_ && (uint32_t)(nowMs - lastPressedMs_) > releaseDelayMs_) {
      pressed_ = false;
    }
    return pressed_;
  }

private:
  uint32_t releaseDelayMs_;
  uint32_t lastPressedMs_ = 0;
  bool pressed_ = false;
};

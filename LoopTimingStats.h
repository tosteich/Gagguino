#pragma once
#include <stdint.h>

// No allocations and no I/O. All times use unsigned modular subtraction.
struct LoopTimingSnapshot {
  uint32_t samples = 0;
  uint32_t maxDurationUs = 0;
  uint32_t avgDurationUs = 0;
  uint32_t maxStartGapUs = 0;
};

class LoopTimingStats {
 public:
  explicit LoopTimingStats(uint32_t reportEveryMs = 10000)
      : reportEveryMs_(reportEveryMs) {}

  void begin(uint32_t startUs, uint32_t nowMs) {
    if (!initialized_) {
      initialized_ = true;
      windowStartMs_ = nowMs;
    } else {
      const uint32_t gap = startUs - previousStartUs_;
      if (gap > maxStartGapUs_) maxStartGapUs_ = gap;
    }
    previousStartUs_ = startUs;
  }

  // Returns true only when a completed report is available.
  bool end(uint32_t endUs, uint32_t nowMs, LoopTimingSnapshot& report) {
    if (!initialized_) return false;
    const uint32_t duration = endUs - previousStartUs_;
    if (duration > maxDurationUs_) maxDurationUs_ = duration;
    totalDurationUs_ += duration;
    ++samples_;
    if ((uint32_t)(nowMs - windowStartMs_) < reportEveryMs_) return false;
    report.samples = samples_;
    report.maxDurationUs = maxDurationUs_;
    report.avgDurationUs = samples_ ? (uint32_t)(totalDurationUs_ / samples_) : 0;
    report.maxStartGapUs = maxStartGapUs_;
    totalDurationUs_ = 0;
    maxDurationUs_ = 0;
    maxStartGapUs_ = 0;
    samples_ = 0;
    windowStartMs_ = nowMs;
    return true;
  }

 private:
  const uint32_t reportEveryMs_;
  uint32_t windowStartMs_ = 0;
  uint32_t previousStartUs_ = 0;
  uint32_t maxDurationUs_ = 0;
  uint32_t maxStartGapUs_ = 0;
  uint64_t totalDurationUs_ = 0;
  uint32_t samples_ = 0;
  bool initialized_ = false;
};

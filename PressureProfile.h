#pragma once
#include <stdint.h>

// Pressure setpoints are stored in tenths of a bar.
// Duration values are seconds; the decline matrix holds at most 60 entries.
struct PressureProfile {
  int preinfuseSeconds;
  int preinfusePressure;
  int holdSeconds;
  int holdPressure;
  int declineSeconds;
  int declinePressure;
};

inline int pressureProfileSetpoint(const PressureProfile& p, uint32_t elapsedMs) {
  const uint32_t elapsedSeconds = elapsedMs / 1000u;
  const uint32_t preinfuse = (uint32_t)(p.preinfuseSeconds > 0 ? p.preinfuseSeconds : 0);
  const uint32_t hold = (uint32_t)(p.holdSeconds > 0 ? p.holdSeconds : 0);
  const uint32_t decline = (uint32_t)(p.declineSeconds > 0 ? p.declineSeconds : 0);
  if (elapsedSeconds < preinfuse) return p.preinfusePressure;
  const uint64_t holdEnd = (uint64_t)preinfuse + hold;
  if ((uint64_t)elapsedSeconds < holdEnd) return p.holdPressure;
  if (decline == 0) return p.holdPressure;
  const uint64_t declineSecond = (uint64_t)elapsedSeconds - holdEnd;
  if (declineSecond >= decline) return p.declinePressure;
  // Match the existing decline matrix: first point makes 1/duration
  // of the total change, final point reaches the target exactly.
  const int64_t delta = (int64_t)p.declinePressure - p.holdPressure;
  return p.holdPressure + (int)(delta * (declineSecond + 1) / decline);
}

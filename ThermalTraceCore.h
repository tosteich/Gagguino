#pragma once
#include <stdint.h>
#include <math.h>

// RAM-only thermal trace. Sampled at the existing 200ms sensor cadence.
// Exactly one completed trace is retained, until the next shot or reboot.
// No flash I/O, no pump/heater side effects, no heap allocation.
class ThermalTraceCore {
public:
  static constexpr uint16_t PRE_SAMPLES = 100;  // 20s
  static constexpr uint16_t FLOW_SAMPLES = 600; // 120s
  static constexpr uint16_t POST_SAMPLES = 300; // 60s
  static constexpr uint16_t MAX_SAMPLES = PRE_SAMPLES + FLOW_SAMPLES + POST_SAMPLES;
  enum Phase : uint8_t { BEFORE=0, BREW=1, AFTER=2 };
  enum State : uint8_t { IDLE=0, FLOW=1, POST=2, READY=3 };
  struct Sample {
    int32_t relativeMs;
    int16_t temperature10;
    uint16_t pressure100;
    uint8_t heater;
    uint8_t pump;
    uint8_t phase;
  };

private:
  struct PreSample {
    uint32_t ms;
    int16_t temperature10;
    uint16_t pressure100;
    uint8_t heater;
    uint8_t pump;
  };
  PreSample pre_[PRE_SAMPLES]{};
  Sample samples_[MAX_SAMPLES]{};
  uint16_t preHead_=0, preCount_=0, count_=0;
  uint16_t flowCount_=0, postCount_=0;
  uint32_t startMs_=0, releaseMs_=0;
  State state_=IDLE;
  bool wasFlow_=false;

  static uint8_t percent(int power) {
    return static_cast<uint8_t>(power < 0 ? 0 : (power > 100 ? 100 : power));
  }
  static int16_t temperature(float c) {
    if (!isfinite(c)) return -500; // same invalid marker as archive
    const long v=lroundf(c*10.0f);
    return static_cast<int16_t>(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
  }
  static uint16_t pressure(float bar) {
    if (!isfinite(bar)) return 0;
    const long v=lroundf(bar*100.0f);
    return static_cast<uint16_t>(v < 0 ? 0 : (v > 65535 ? 65535 : v));
  }
  void append(uint32_t now, const PreSample& reading, Phase phase) {
    if (count_>=MAX_SAMPLES) return;
    const int32_t rel=static_cast<int32_t>(now - startMs_);
    samples_[count_++]={rel,reading.temperature10,reading.pressure100,
                        reading.heater,reading.pump,static_cast<uint8_t>(phase)};
  }
  void start(uint32_t now) {
    count_=flowCount_=postCount_=0;
    startMs_=now;
    for(uint16_t i=0;i<preCount_;++i) {
      const uint16_t index=static_cast<uint16_t>((preHead_+PRE_SAMPLES-preCount_+i)%PRE_SAMPLES);
      const PreSample& reading=pre_[index];
      append(reading.ms,reading,BEFORE);
    }
    state_=FLOW;
  }

public:
  State state() const { return state_; }
  uint16_t count() const { return count_; }
  const Sample& at(uint16_t index) const { return samples_[index]; }
  void sample(bool flowing, uint32_t now, float tempC, float pressureBar,
              int heatPercent, int pumpPercent) {
    const PreSample reading={now,temperature(tempC),pressure(pressureBar),
                             percent(heatPercent),percent(pumpPercent)};
    if(flowing && !wasFlow_) start(now);
    if(state_==FLOW && !flowing && wasFlow_) {
      state_=POST;
      releaseMs_=now;
    }
    if(state_==FLOW && flowing && flowCount_<FLOW_SAMPLES) {
      append(now,reading,BREW);
      ++flowCount_;
    } else if(state_==POST && postCount_<POST_SAMPLES) {
      append(now,reading,AFTER);
      ++postCount_;
      if((uint32_t)(now-releaseMs_)>=60000u || postCount_==POST_SAMPLES) state_=READY;
    }
    wasFlow_=flowing;
    // Continuously collect pre-roll data even while ready or recording.
    pre_[preHead_]=reading;
    preHead_=static_cast<uint16_t>((preHead_+1)%PRE_SAMPLES);
    if(preCount_<PRE_SAMPLES)++preCount_;
  }
};

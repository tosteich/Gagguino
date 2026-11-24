#pragma once
#include <Arduino.h>

class Timer {
public:
  Timer() : _tmr(0), _period(0) {}

  Timer(int period) : _tmr(0), _period(period) {}

  bool ready() {
    if (_period <= 0) {
      return false;
    }

    const uint32_t now = millis();
    if ((uint32_t)(now - _tmr) >= (uint32_t)_period) {
      _tmr = now;
      return true;
    }
    return false;
  }

  void setPeriod(int period) {
    _period = period;
    _tmr = millis();
  }

private:
  uint32_t _tmr;
  int _period;
};

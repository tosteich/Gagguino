// Native GPIO-stub regression tests for Boiler transitions.
// g++ -std=c++17 -Wall -Wextra -Werror tests/boiler_power_transition_test.cpp -o /tmp/gagguino_boiler_transition && /tmp/gagguino_boiler_transition
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>
using boolean = bool;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
static unsigned long nowMs = 0;
static int outputState = -1;
static int outputWrites = 0;
unsigned long millis() { return nowMs; }
void digitalWrite(int pin, int value) { assert(pin == 14); outputState = value; ++outputWrites; }
void pinMode(int pin, int value) { assert(pin == 14 && value == OUTPUT); }
template <typename T> T constrain(T x, T lo, T hi) { return std::max(lo, std::min(x, hi)); }
#include "../Boiler.h"

int main() {
  Boiler boiler(1000);
  boiler.begin();
  assert(outputState == LOW);

  // 0% and 100% must act immediately, without waiting for a PWM phase.
  boiler.setHeatPower(100);
  assert(outputState == HIGH);
  boiler.setHeatPower(0);
  assert(outputState == LOW);
  nowMs = 1000;
  boiler.updateHeater();
  assert(outputState == LOW);

  // 50% starts from OFF; after 500 ms turns ON, after 500 ms OFF.
  boiler.setHeatPower(50);
  assert(outputState == LOW);
  nowMs = 1499;
  boiler.updateHeater();
  assert(outputState == LOW);
  nowMs = 1500;
  boiler.updateHeater();
  assert(outputState == HIGH);
  nowMs = 1999;
  boiler.updateHeater();
  assert(outputState == HIGH);
  nowMs = 2000;
  boiler.updateHeater();
  assert(outputState == LOW);

  // Changing a partial duty must not energize immediately or alter safety cutoff.
  boiler.setHeatPower(80);
  assert(outputState == LOW);
  boiler.setHeatPower(0);
  assert(outputState == LOW);
  nowMs += 10000;
  boiler.updateHeater();
  assert(outputState == LOW);

  // Leaving full ON for partial duty must eventually turn OFF.
  boiler.setHeatPower(100);
  assert(outputState == HIGH);
  boiler.setHeatPower(10);
  nowMs += 100;
  boiler.updateHeater();
  assert(outputState == LOW);

  // Disabled period always forces OFF.
  boiler.setHeatPower(100);
  assert(outputState == HIGH);
  boiler.setPeriod(0);
  assert(outputState == LOW);
  boiler.setHeatPower(100);
  assert(outputState == LOW);

  // millis() wraparound should not leave the output latched ON.
  boiler.setPeriod(1000);
  boiler.setHeatPower(100);
  assert(outputState == HIGH);
  nowMs = 0xFFFFFF00UL;
  boiler.setHeatPower(50);
  nowMs = 0xFFFFFF00UL + 500UL; // native unsigned long may be 64-bit; normalize below
  nowMs = (unsigned long)(uint32_t)nowMs;
  boiler.updateHeater();
  assert(outputState == LOW);

  std::cout << "Boiler power transition scenarios passed\n";
}

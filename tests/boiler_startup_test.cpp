// Compile: g++ -std=c++17 -Wall -Wextra -Werror tests/boiler_startup_test.cpp -o /tmp/boiler_startup_test && /tmp/boiler_startup_test
#include <cassert>
#include <iostream>
#include <algorithm>
using boolean = bool;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
static unsigned long ticks = 123;
static int gpioWrites = 0;
static int gpioModes = 0;
static int outputLevel = -1;
unsigned long millis() { return ticks; }
void digitalWrite(int pin, int level) { assert(pin == 14); outputLevel = level; ++gpioWrites; }
void pinMode(int pin, int mode) { assert(pin == 14 && mode == OUTPUT); ++gpioModes; }
template <typename T> T constrain(T x, T lo, T hi) { return std::max(lo, std::min(hi, x)); }
#include "../Boiler.h"

int main() {
  Boiler boiler(1000);
  assert(gpioWrites == 0 && gpioModes == 0); // constructor has no GPIO effects
  boiler.setHeatPower(100);
  assert(gpioWrites == 0); // not initialized: cannot energize GPIO
  boiler.begin();
  assert(gpioModes == 1 && outputLevel == LOW);
  boiler.setHeatPower(100);
  assert(outputLevel == HIGH);
  boiler.setHeatPower(0);
  assert(outputLevel == LOW);
  std::cout << "Boiler initialization safety checks passed\n";
}

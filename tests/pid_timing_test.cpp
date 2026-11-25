// g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_timing_test.cpp -o /tmp/pid_timing_test && /tmp/pid_timing_test
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  // A 400 ms interval must integrate twice as much as a 200 ms interval.
  GaggiaPid measured(200), fixed(200);
  measured.setupPid(0, 1, 0, 100);
  fixed.setupPid(0, 1, 0, 100);
  assert(measured.getResultAt(90, 1000) == 2);
  assert(measured.getResultAt(90, 1400) == 6);
  assert(fixed.getResult(90) == 2);
  assert(fixed.getResult(90) == 4);

  // Derivative is divided by actual elapsed time, not the nominal period.
  GaggiaPid derivative(200);
  derivative.setupPid(0, 0, 9, 93);
  assert(derivative.getResultAt(90, 1000) == 0);
  assert(derivative.getResultAt(91, 1400) == 22); // 9 * 1 / 0.4
  derivative.resetState();
  assert(derivative.getResultAt(90, 2000) == 0);
  assert(derivative.getResultAt(91, 2200) == 45);

  // Arithmetic wraps correctly over uint32_t millis() rollover.
  GaggiaPid wrapping(200);
  wrapping.setupPid(0, 1, 0, 100);
  assert(wrapping.getResultAt(90, UINT32_MAX - 99) == 2);
  assert(wrapping.getResultAt(90, 100) == 4); // 200 ms elapsed.

  // A long scheduling gap must not produce a derivative spike.
  GaggiaPid gap(200);
  gap.setupPid(0, 0, 9, 100);
  assert(gap.getResultAt(90, 1000) == 0);
  assert(gap.getResultAt(91, 8000) == 0);

  // A bad sensor value interrupts derivative history.
  GaggiaPid invalid(200);
  invalid.setupPid(0, 0, 9, 100);
  assert(invalid.getResultAt(90, 1000) == 0);
  assert(invalid.getResultAt(NAN, 1200) == 0);
  assert(invalid.getResultAt(91, 1400) == 0);

  std::cout << "Measured PID timing tests passed\\n";
}

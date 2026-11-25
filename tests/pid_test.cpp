// Characterization tests for the original Gagguino fixed-step PID.
#include <cassert>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  GaggiaPid integral(200);
  integral.setupPid(0, 1, 0, 100);
  assert(integral.getResult(90) == 2);
  assert(integral.getResult(90) == 4);
  integral.resetState();
  assert(integral.getResult(90) == 2);

  // Original D sign is positive with rising measurement.
  GaggiaPid derivative(200);
  derivative.setupPid(0, 0, 1, 100);
  assert(derivative.getResult(100) == 100);
  assert(derivative.getResult(101) == 5);

  GaggiaPid saturated(20);
  saturated.setupPid(3, 5, 0, 100);
  for (int i = 0; i < 200; ++i) {
    assert(saturated.getResult(0) == 100);
  }
  assert(saturated.getResult(100) >= 0);

  GaggiaPid invalid(200);
  invalid.setupPid(1, 1, 1, 90);
  assert(invalid.getResult(NAN) == 0);
  std::cout << "Legacy PID characterization passed\n";
}

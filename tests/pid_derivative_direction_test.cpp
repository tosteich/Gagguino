// Compile: g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_derivative_direction_test.cpp -o /tmp/gagguino_pid_derivative_direction && /tmp/gagguino_pid_derivative_direction
#include <cassert>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  GaggiaPid legacyPump(200);
  legacyPump.setupPid(0, 0, 1, 95);
  assert(legacyPump.getResult(90) == 0);
  assert(legacyPump.getResult(91) == 5);
  assert(legacyPump.getResult(90) == 0);

  GaggiaPid boiler(200);
  boiler.setDerivativeOpposesMeasurement(true);
  boiler.setupPid(0, 0, 1, 95);
  assert(boiler.getResult(90) == 0);
  assert(boiler.getResult(91) == 0);
  assert(boiler.getResult(90) == 5);

  GaggiaPid withP(200);
  withP.setDerivativeOpposesMeasurement(true);
  withP.setupPid(2, 0, 1, 95);
  assert(withP.getResult(90) == 10);
  assert(withP.getResult(91) == 3);
  assert(withP.getResult(90) == 15);

  // Switching direction discards derivative state, avoiding an old-sample kick.
  GaggiaPid switched(200);
  switched.setupPid(0, 0, 1, 95);
  switched.getResult(90);
  switched.getResult(91);
  switched.setDerivativeOpposesMeasurement(true);
  assert(switched.getResult(90) == 0);
  assert(switched.getResult(89) == 5);

  std::cout << "PID derivative direction checks passed\n";
}

// Run from repository root:
// g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_state_reset_test.cpp -o /tmp/gagguino_pid_state_reset && /tmp/gagguino_pid_state_reset
#include <cassert>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  GaggiaPid pid(200);
  pid.setupPid(0.0f, 1.0f, 0.0f, 100.0f);
  for (int i = 0; i < 20; ++i) pid.getResult(90.0f);
  pid.resetState();
  assert(pid.getResult(90.0f) == 2);
  pid.setSetPoint(80.0f);
  pid.resetState();
  assert(pid.getResult(90.0f) == 0);

  GaggiaPid derivative(200);
  derivative.setupPid(0.0f, 0.0f, 1.0f, 100.0f);
  assert(derivative.getResult(90.0f) == 0);
  assert(derivative.getResult(91.0f) == 5);
  derivative.resetState();
  assert(derivative.getResult(90.0f) == 0);

  std::cout << "PID state reset scenarios passed\n";
}

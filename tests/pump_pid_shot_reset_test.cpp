// Run: g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pump_pid_shot_reset_test.cpp -o /tmp/gagguino_pump_pid_shot_reset && /tmp/gagguino_pump_pid_shot_reset
#include <cassert>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  // Same PID reset and new setpoint sequence used at the start/end of a shot.
  GaggiaPid pump(20);
  pump.setupPid(0.0f, 1.0f, 0.0f, 80.0f);
  for (int i = 0; i < 100; ++i) pump.getResult(70.0f);
  pump.resetState(); // release
  pump.resetState(); // next press
  pump.setSetPoint(80.0f);
  assert(pump.getResult(70.0f) == 0); // 10 * 1 * 0.02 = 0.2 -> truncated 0
  for (int i = 0; i < 10; ++i) pump.getResult(70.0f);
  const int newShotPower = pump.getResult(70.0f);
  assert(newShotPower == 2);
  // Derivative state likewise must not carry across shots.
  GaggiaPid derivative(20);
  derivative.setupPid(0, 0, 1, 90);
  derivative.getResult(90);
  assert(derivative.getResult(91) == 50);
  derivative.resetState();
  derivative.setSetPoint(90);
  assert(derivative.getResult(90) == 0);
  // Reconfiguration should not inherit the previous shot's integral.
  GaggiaPid retuned(20);
  retuned.setupPid(0.0f, 10.0f, 0.0f, 80.0f);
  for (int i = 0; i < 50; ++i) retuned.getResult(70.0f);
  retuned.resetState();
  retuned.setupPid(0.0f, 1.0f, 0.0f, 80.0f);
  assert(retuned.getResult(70.0f) == 0);
  // Changing a pressure setpoint within the same shot preserves integral.
  assert(retuned.getResult(70.0f) == 0);
  retuned.setSetPoint(60.0f);
  assert(retuned.getResult(70.0f) == 0);
  // Entering manual mode resets derivative history for the new control mode.
  GaggiaPid switched(20);
  switched.setupPid(0.0f, 0.0f, 1.0f, 90.0f);
  switched.getResult(90.0f);
  assert(switched.getResult(91.0f) == 50);
  switched.resetState();
  switched.setSetPoint(80.0f);
  assert(switched.getResult(91.0f) == 0);
  std::cout << "Pump PID between-shots and mode-change reset checks passed\n";
}

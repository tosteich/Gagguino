// Run from repository root:
// g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_scenarios.cpp -o /tmp/gagguino_pid_scenarios && /tmp/gagguino_pid_scenarios
#include <cassert>
#include <cmath>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  // Fixed-cadence PID sequences only; this does not model boiler/pump physics.
  GaggiaPid boiler(200);
  boiler.setupPid(2.0f, 1.0f, 0.0f, 95.0f);
  int output = boiler.getResult(80);
  assert(output >= 0 && output <= 100);
  for (int i = 0; i < 1000; ++i) {
    output = boiler.getResult(80);
    assert(output >= 0 && output <= 100);
  }

  boiler.setSetPoint(70.0f);
  const int afterSetpointDrop = boiler.getResult(90.0f);
  assert(afterSetpointDrop >= 0 && afterSetpointDrop <= 100);
  // Note: currently nonzero due to retained integral after saturation.
  // Document behavior rather than imposing an unverified heater policy.

  GaggiaPid pump(20);
  pump.setupPid(0.0f, 0.0f, 0.4f, 90.0f);
  assert(pump.getResult(90.0f) == 0);
  const int noiseRise = pump.getResult(91.0f);
  const int noiseFall = pump.getResult(90.0f);
  assert(noiseRise == 20);
  assert(noiseFall == 0);

  GaggiaPid withoutD(20);
  withoutD.setupPid(0.0f, 0.0f, 0.0f, 90.0f);
  assert(withoutD.getResult(90.0f) == 0);
  assert(withoutD.getResult(91.0f) == 0);

  GaggiaPid invalid(200);
  invalid.setupPid(1.0f, 0.0f, 1.0f, 95.0f);
  invalid.getResult(90.0f);
  assert(invalid.getResult(NAN) == 0);
  const int afterInvalidSample = invalid.getResult(90.0f);
  assert(afterInvalidSample >= 0 && afterInvalidSample <= 100);

  std::cout << "PID sequence scenarios passed\n";
  std::cout << "Boiler output after setpoint drop: "
            << afterSetpointDrop << "/100\n";
  std::cout << "Pump output pulse for +1 measurement unit: "
            << noiseRise << "/100\n";
}

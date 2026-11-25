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

  // A direct setSetPoint() deliberately preserves accumulated integral.
  // This is used by the pump while its profile moves through pressure stages.
  boiler.setSetPoint(70.0f);
  const int afterSetpointDropWithoutReset = boiler.getResult(90.0f);
  assert(afterSetpointDropWithoutReset >= 0 && afterSetpointDropWithoutReset <= 100);

  // The application resets boiler PID before updating its setpoint
  // (web/Nextion settings and brew/steam transitions). Reproduce that
  // sequence explicitly: old integral must never cause heating above target.
  boiler.resetState();
  boiler.setSetPoint(70.0f);
  const int afterSetpointDropWithReset = boiler.getResult(90.0f);
  assert(afterSetpointDropWithReset == 0);

  // Verify new target works again on a subsequent valid low measurement.
  const int belowNewTarget = boiler.getResult(65.0f);
  assert(belowNewTarget > 0 && belowNewTarget <= 100);

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
  std::cout << "Boiler output after direct setpoint drop without reset: "
            << afterSetpointDropWithoutReset << "/100\n";
  std::cout << "Boiler output after application-style PID reset: "
            << afterSetpointDropWithReset << "/100\n";
  std::cout << "Pump output pulse for +1 measurement unit: "
            << noiseRise << "/100\n";
}

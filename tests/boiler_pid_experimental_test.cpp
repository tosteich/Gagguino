// Native-only characterization: experimental boiler PID must remain isolated.
#include <cassert>
#include <cmath>
#include <iostream>
#include "../BoilerPid.h"
#include "../GaggiaPid.h"

int main() {
  // Rising temperature should reduce heating; cooling should increase it.
  BoilerPid derivative(200);
  derivative.setupPid(0, 0, 9, 93);
  assert(derivative.getResult(92) == 0); // no D spike on first sample
  assert(derivative.getResult(91) == 45); // -1 degree / .2 s => +45%
  assert(derivative.getResult(92) == 0); // +1 degree / .2 s => -45%
  derivative.resetState();
  assert(derivative.getResult(92) == 0);

  // The PID must not wind up when an error cannot be corrected by the heater.
  BoilerPid saturated(200);
  saturated.setupPid(60, 5, 0, 93);
  for (int i=0; i<500; ++i) assert(saturated.getResult(90)==100);
  assert(saturated.getResult(93)==0);

  // Integration is allowed when not at the output limits.
  BoilerPid integral(200);
  integral.setupPid(0, 1, 0, 93);
  assert(integral.getResult(92)==0); // float-to-int truncation (0.2)
  for (int i=0; i<4; ++i) integral.getResult(92);
  assert(integral.getResult(92)==1);
  integral.setSetPoint(90);
  assert(integral.getResult(93) == 0); // integral can unwind

  // Faults and mode changes clear controller memory.
  BoilerPid invalid(200);
  invalid.setupPid(6, .2f, 9, 93);
  assert(invalid.getResult(NAN)==0);
  assert(invalid.getResult(INFINITY)==0);
  assert(invalid.getResult(92)>=0);
  invalid.resetState();
  assert(invalid.getResult(92)>=0);

  // Keep the legacy pump mathematics unmodified, including original D sign.
  GaggiaPid pump(20);
  pump.setupPid(0, 0, 1, 90);
  pump.resetState();
  assert(pump.getResult(90)==0);
  assert(pump.getResult(91)==50);

  std::cout << "Experimental boiler PID isolation and safety tests passed\n";
}

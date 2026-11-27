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
  assert(derivative.getResult(91) == 5); // filtered: -5 C/s -> +5.29%
  assert(derivative.getResult(92) == 0); // rising temperature suppresses heat
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

  // Extended warm-up at 90 C must never accumulate more than 20% integral.
  BoilerPid warmup(200);
  warmup.setupPid(6, .2f, 9, 93);
  for (int i=0;i<2000;++i) warmup.getResult(90);
  assert(warmup.getITerm()<=20.0001f);
  // Once the temperature exceeds target, old integral must bleed away.
  float initialIntegral=warmup.getITerm();
  for (int i=0;i<50;++i) warmup.getResult(93.1f);
  assert(warmup.getITerm()<initialIntegral*0.25f);

  // One sensor jump must not generate a huge derivative spike.
  BoilerPid filtered(200);
  filtered.setupPid(6, .2f, 9, 93);
  filtered.getResult(93);
  filtered.getResult(92);
  assert(filtered.getDTerm()>0 && filtered.getDTerm()<10.0f);
  filtered.getResult(100);
  assert(filtered.getDTerm()>=-30.0f && filtered.getDTerm()<=30.0f);
  filtered.resetState();
  filtered.getResult(93);
  assert(filtered.getDTerm()==0);

  // Steady temperature at/above target should not accumulate positive integral.
  BoilerPid steady(200);
  steady.setupPid(6, .2f, 9, 93);
  for (int i=0;i<2000;++i) steady.getResult(93.2f);
  assert(steady.getITerm()==0.0f);

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

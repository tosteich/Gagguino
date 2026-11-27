#include <cassert>
#include <iostream>
#include <type_traits>
#include "../BenchTestConfig.h"
#include "../GaggiaPid.h"
#include "../BoilerPid.h"

#ifdef GAGGUINO_EXPERIMENTAL_BOILER_PID
using ActiveBoilerPid = BoilerPid;
using ShadowBoilerPid = GaggiaPid;
#else
using ActiveBoilerPid = GaggiaPid;
using ShadowBoilerPid = BoilerPid;
#endif
int main() {
  static_assert(!std::is_same<ActiveBoilerPid, ShadowBoilerPid>::value,
                "Active and shadow boiler algorithms must differ");
  ActiveBoilerPid active(200);
  ShadowBoilerPid shadow(200);
  GaggiaPid pump(20);
  active.setupPid(6.0f,0.2f,9.0f,93.0f);
  shadow.setupPid(6.0f,0.2f,9.0f,93.0f);
  pump.setupPid(12.0f,1.4f,0.0f,90.0f);
  assert(active.getResult(93.0f)>=0);
  assert(shadow.getResult(93.0f)>=0);
  assert(active.getSetPoint()==93.0f);
  // Pump implementation, tuning and output are always legacy.
  assert(pump.getResult(90.0f)==0);
  assert(pump.getKd()==0.0f);
  std::cout << "Boiler PID selection and unchanged pump compile test passed\n";
}

// Run: g++ -std=c++17 -Wall -Wextra -Werror -I. tests/pump_phase_plan_test.cpp -o /tmp/pump_phase_plan_test && /tmp/pump_phase_plan_test
#include <cassert>
#include <iostream>
#include "../PumpPhasePlan.h"
int main() {
  PumpPhasePlan plan;
  assert(!plan.takePulse()); // No pulse at boot, before a zero crossing.
  for (int p = 0; p <= 100; ++p) {
    const uint32_t d = plan.zeroCross(p);
    assert(d == static_cast<uint32_t>(49000 - 478.5 * p));
    assert(d >= PumpPhasePlan::kMinTicks && d <= PumpPhasePlan::kMaxTicks);
    assert(plan.takePulse());
    assert(!plan.takePulse()); // Never repeat without another zero crossing.
  }
  assert(plan.zeroCross(-10) == 49000);
  assert(plan.zeroCross(110) == 1150);
  plan.cancel();
  assert(!plan.takePulse()); // Lost sync or explicit cancellation.
  assert(plan.zeroCross(50) == static_cast<uint32_t>(49000 - 478.5 * 50));
  assert(plan.takePulse());
  assert(!plan.takePulse());
  std::cout << "Pump phase plan tests passed\n";
}

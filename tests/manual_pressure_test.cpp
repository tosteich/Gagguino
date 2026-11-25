// Compiled by tests/run_native_tests.sh
#include <cassert>
#include <cstdint>
#include <climits>
#include <iostream>
#include "../ManualPressure.h"

int main() {
  assert(manualPressureTenthsFromNextion(-1) == 0);
  assert(manualPressureTenthsFromNextion(0) == 0);
  assert(manualPressureTenthsFromNextion(1) == 5);
  assert(manualPressureTenthsFromNextion(2) == 10);
  assert(manualPressureTenthsFromNextion(18) == 90);
  assert(manualPressureTenthsFromNextion(24) == 120);
  assert(manualPressureTenthsFromNextion(25) == 120);
  assert(manualPressureTenthsFromNextion(INT32_MAX) == 120);
  assert(manualPressureTenthsFromNextion(UINT32_MAX) == 120);
  assert(manualPressureTenthsFromNextion(INT64_MAX) == 120);
  assert(manualPressureTenthsFromNextion(INT64_MIN) == 0);
  std::cout << "Manual pressure input bounds passed\n";
}

// Build from repo root:
// g++ -std=c++17 -Wall -Wextra -Werror tests/pump_delay_test.cpp -o /tmp/gagguino_pump_delay_test && /tmp/gagguino_pump_delay_test
#include <cassert>
#include <iostream>

int previousDelay(int value) { return 49000 - 478.5 * value; }
int boundedDelay(int value) {
  const int clamped = value < 0 ? 0 : (value > 100 ? 100 : value);
  return 49000 - 478.5 * clamped;
}

int main() {
  for (int x = 0; x <= 100; ++x) {
    assert(boundedDelay(x) == previousDelay(x));
    assert(boundedDelay(x) >= 1150 && boundedDelay(x) <= 49000);
  }
  assert(boundedDelay(-500) == previousDelay(0));
  assert(boundedDelay(200) == previousDelay(100));
  std::cout << "Pump phase delay bounds passed\n";
}

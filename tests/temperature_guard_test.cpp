// Compile from repo root:
// g++ -std=c++17 -Wall -Wextra -Werror tests/temperature_guard_test.cpp -o /tmp/temperature_guard_test && /tmp/temperature_guard_test
#include "../TemperatureGuard.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
int main() {
  TemperatureGuard sensor;
  assert(!sensor.fresh(0));                      // No sensor at boot
  assert(!sensor.accept(100, 10));                // Invalid / disconnected
  assert(!sensor.fresh(11));
  assert(!sensor.accept(1200, 100));              // First plausible burst
  assert(!sensor.fresh(100));                     // Needs second plausible burst
  assert(sensor.accept(1200, 200));               // 25 C
  assert(std::fabs(sensor.temperatureC()-25.0f)<0.001f);
  assert(sensor.fresh(1200));                     // Within 1000ms
  assert(!sensor.fresh(1201));                    // Stale reading
  assert(!sensor.accept(1200, 1500));             // Requalify after timeout
  assert(!sensor.fresh(1500));
  assert(sensor.accept(1216, 1600));              // 26 C
  assert(sensor.fresh(1600));
  assert(!sensor.accept(2500, 1700));             // Implausible jump
  assert(!sensor.fresh(1700));
  assert(sensor.accept(2500, 1800));              // Requalifies after reset
  assert(sensor.fresh(1800));
  assert(!sensor.accept(3105, 1900));             // Outside allowed range
  assert(!sensor.fresh(1900));
  TemperatureGuard candidateTimeout;
  assert(!candidateTimeout.accept(1200, 100));
  assert(!candidateTimeout.accept(1200, 1200)); // Two frames too far apart
  assert(!candidateTimeout.fresh(1200));
  assert(candidateTimeout.accept(1200, 1300));
  assert(candidateTimeout.fresh(1300));
  assert(!sensor.accept(800, UINT32_MAX - 20));
  assert(sensor.accept(800, 10));                 // millis wrap
  assert(sensor.fresh(20));
  assert(!sensor.fresh(1100));
  std::cout << "Temperature guard tests passed\n";
}

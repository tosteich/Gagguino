// Run from repository root:
// g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_test.cpp -o /tmp/gagguino_pid_test && /tmp/gagguino_pid_test
#include <cassert>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  GaggiaPid first(200);
  first.setupPid(0, 0, 1, 100);
  assert(first.getResult(100) == 0); // No derivative kick on first sample.
  assert(first.getResult(101) == 5); // Historical D sign retained.

  GaggiaPid integral(200);
  integral.setupPid(2, 10, 0, 100);
  assert(integral.getResult(80) == 80);
  assert(integral.getResult(80) == 100);
  for (int i = 0; i < 20; ++i) integral.getResult(110);
  assert(integral.getResult(110) == 0);

  GaggiaPid saturated(20);
  saturated.setupPid(3, 5, 0, 100);
  for (int i = 0; i < 3000; ++i) assert(saturated.getResult(0) == 100);
  assert(saturated.getResult(100) == 0);

  GaggiaPid updated(200);
  updated.setupPid(0, 1, 0, 100);
  assert(updated.getResult(90) == 2);
  updated.setupPid(0, 1, 0, 100);
  assert(updated.getResult(90) == 4);

  GaggiaPid invalid(0);
  invalid.setupPid(1, 0, 0, 100);
  assert(invalid.getResult(50) == 0);

  std::cout << "PID tests passed\n";
}

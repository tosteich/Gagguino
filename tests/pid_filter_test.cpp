// Run from repository root:
// g++ -std=c++17 -Wall -Wextra -Werror -Itests -I. tests/pid_filter_test.cpp -o /tmp/gagguino_pid_filter_test && /tmp/gagguino_pid_filter_test
#include <cassert>
#include <cmath>
#include <iostream>
#include "../GaggiaPid.h"

int main() {
  GaggiaPid unfiltered(20), filtered(20);
  unfiltered.setupPid(0, 0, 0.4f, 90);
  filtered.setupPid(0, 0, 0.4f, 90);
  filtered.setDerivativeFilterAlpha(0.1f);
  assert(unfiltered.getResult(90) == 0);
  assert(filtered.getResult(90) == 0);
  assert(unfiltered.getResult(91) == 20);
  assert(filtered.getResult(91) == 2);
  assert(unfiltered.getResult(90) == 0);
  assert(filtered.getResult(90) == 0);

  GaggiaPid state(200);
  state.setupPid(0, 1, 0, 100);
  assert(state.getResult(90) == 2);
  assert(state.getResult(90) == 4);
  state.resetState();
  assert(state.getResult(90) == 2);
  state.setDerivativeFilterAlpha(0.0f); // invalid alpha ignored
  state.setDerivativeFilterAlpha(NAN);
  assert(state.getResult(90) == 4);

  GaggiaPid nanParam(20);
  nanParam.setupPid(NAN, 1, 0, 90);
  assert(nanParam.getResult(85) == 0);
  std::cout << "PID opt-in filter and reset tests passed\n";
}

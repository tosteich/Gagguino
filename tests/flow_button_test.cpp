// Run from repository root:
// g++ -std=c++17 -Wall -Wextra -Werror -I. tests/flow_button_test.cpp -o /tmp/gagguino_flow_button_test && /tmp/gagguino_flow_button_test
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../FlowButtonState.h"

int main() {
  FlowButtonState button(50);
  assert(!button.update(false, 0));
  assert(button.update(true, 100));       // Immediate press.
  assert(button.update(false, 130));     // Brief release ignored.
  assert(button.update(true, 140));      // Press resumes.
  assert(button.update(false, 190));     // Exactly 50 ms: still pressed.
  assert(!button.update(false, 191));    // More than 50 ms: released.
  assert(button.update(true, 200));      // Re-press immediately.

  FlowButtonState rollover(50);
  assert(rollover.update(true, UINT32_MAX - 20));
  assert(rollover.update(false, 10));    // Wraparound: only 31 ms passed.
  assert(!rollover.update(false, 40));   // 61 ms passed.
  std::cout << "Flow button debounce tests passed\n";
}

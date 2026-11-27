#include <cassert>
#include <cmath>
#include <iostream>
#include "../BoilerOvertempLatch.h"

int main() {
  BoilerOvertempLatch normal;
  assert(!normal.observe(93.0f,93.0f));
  assert(!normal.observe(97.9f,93.0f));
  assert(normal.observe(98.0f,93.0f));
  assert(normal.observe(90.0f,93.0f)); // never reenable in this boot

  BoilerOvertempLatch steam;
  assert(!steam.observe(135.0f,135.0f));
  assert(steam.observe(140.0f,135.0f));

  BoilerOvertempLatch invalid;
  assert(invalid.observe(NAN,93.0f));

  BoilerOvertempLatch target;
  assert(target.observe(92.0f,0.0f));

  BoilerOvertempLatch selection;
  assert(selection.observe(97.0f,92.0f));
  std::cout << "Experimental boiler overtemperature latch passed\n";
}

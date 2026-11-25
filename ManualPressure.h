#pragma once
#include <stdint.h>

// Nextion manualPressure stores half-bar units: 1 -> 0.5 bar = 5 tenths.
// Validate BEFORE multiplication, including malformed negative/huge values.
// Matches the 12-bar upper bound used for profile pressure settings.
inline int manualPressureTenthsFromNextion(int64_t halfBarUnits) {
  if (halfBarUnits <= 0) return 0;
  if (halfBarUnits >= 24) return 120;
  return static_cast<int>(halfBarUnits * 5);
}

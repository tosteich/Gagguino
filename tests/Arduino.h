#pragma once
#include <algorithm>
#include <cmath>
using std::isfinite;
// Arduino's constrain macro permits mixed numeric bound types. Native tests
// must also accept float values with integer 0/100 bounds from legacy PID.
template<typename T, typename L, typename H> T constrain(T value, L low, H high) {
  return std::max(static_cast<T>(low), std::min(value, static_cast<T>(high)));
}
template<typename T> T max(T a, T b) { return std::max(a, b); }
template<typename T> T min(T a, T b) { return std::min(a, b); }

#pragma once
#include <algorithm>
#include <cmath>
using std::isfinite;
template<typename T> T constrain(T value, T low, T high) {
  return std::max(low, std::min(value, high));
}
template<typename T> T max(T a, T b) { return std::max(a, b); }
template<typename T> T min(T a, T b) { return std::min(a, b); }

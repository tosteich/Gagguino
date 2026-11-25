// Native test: bash tests/run_native_tests.sh
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../LoopTimingStats.h"

int main() {
  LoopTimingStats stats(10000);
  LoopTimingSnapshot sample;
  assert(!stats.end(10, 0, sample));
  stats.begin(1000, 100);
  assert(!stats.end(1200, 100, sample));
  stats.begin(3000, 101);
  assert(!stats.end(3300, 101, sample));
  stats.begin(9000, 102);
  assert(stats.end(9600, 10100, sample));
  assert(sample.samples == 3);
  assert(sample.maxDurationUs == 600);
  assert(sample.avgDurationUs == 366); // (200+300+600)/3
  assert(sample.maxStartGapUs == 6000);
  stats.begin(11000, 10101);
  assert(!stats.end(11100, 10101, sample));
  stats.begin(12000, 20101);
  assert(stats.end(12100, 20101, sample));
  assert(sample.samples == 2);
  assert(sample.maxDurationUs == 100);
  assert(sample.avgDurationUs == 100);
  assert(sample.maxStartGapUs == 1000);
  // micros() rollover: elapsed calculations must remain correct.
  LoopTimingStats rollover(10);
  rollover.begin(UINT32_MAX - 10, UINT32_MAX - 1);
  assert(rollover.end(9, UINT32_MAX - 1, sample) == false);
  rollover.begin(19, 0);
  assert(rollover.end(29, 10, sample));
  assert(sample.samples == 2);
  assert(sample.maxDurationUs == 20);
  assert(sample.avgDurationUs == 15);
  assert(sample.maxStartGapUs == 30);
  std::cout << "Loop timing stats tests passed\n";
}

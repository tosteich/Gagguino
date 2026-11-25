// Compile: g++ -std=c++17 -Wall -Wextra -Werror -I. tests/stopwatch_rollover_test.cpp -o /tmp/gagguino_stopwatch_test && /tmp/gagguino_stopwatch_test
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
using byte = unsigned char;
using boolean = bool;
static uint32_t fakeMillis = 0;
uint32_t millis() { return fakeMillis; }
struct AsyncWebSocket {
  int messages = 0;
  void textAll(const char *) { ++messages; }
};
struct EasyNex {
  int writes = 0;
  void writeNum(const char *, uint32_t) { ++writes; }
  void writeStr(const char *) { ++writes; }
};
#include "../StopWatch.h"

int main() {
  AsyncWebSocket ws;
  EasyNex nex;
  StopWatch shot;
  assert(shot.getTimerTime() == 0);
  // Before the first shot, currentTime is uptime from power-on.
  assert(shot.getCurrentTimeSeconds(12345u) == 12u);
  shot.setIsFlow(true);
  shot.setFlowJustPressed(true);
  fakeMillis = UINT32_MAX - 100u;
  shot.run(ws, nex);
  assert(shot.getTimerTime() == 0);
  assert(shot.getResetTime() == UINT32_MAX - 100u);
  shot.run(ws, nex);  // same time, must not restart shot
  fakeMillis = UINT32_MAX - 50u;
  shot.run(ws, nex);
  assert(shot.getTimerTime() == 50u);
  assert(shot.getCurrentTimeSeconds(fakeMillis) == 0u);
  fakeMillis = 49u; // 150 ms across millis() rollover
  shot.run(ws, nex);
  assert(shot.getTimerTime() == 150u);
  assert(shot.getResetTime() == UINT32_MAX - 100u);
  assert(shot.getCurrentTimeSeconds(fakeMillis) == 0u);
  // The flowJustPressed UI handshake must not shift the shot clock.
  shot.setIsFlow(false);
  shot.setFlowJustPressed(true);
  shot.run(ws, nex);
  // Shot duration freezes, but currentTime restarts at confirmed release.
  assert(shot.getTimerTime() == 150u);
  assert(shot.getCurrentTimeSeconds(fakeMillis) == 0u);
  fakeMillis = 5000u;
  shot.run(ws, nex);
  assert(shot.getCurrentTimeSeconds(fakeMillis) == 4u);
  assert(shot.getTimerTime() == 150u);
  for (int i = 0; i < 45; ++i) shot.run(ws, nex);
  assert(shot.getTimerTime() == 0u);
  shot.setIsFlow(true);
  fakeMillis = 100u;
  shot.run(ws, nex);
  assert(shot.getTimerTime() == 0u);  // next shot starts cleanly
  assert(shot.getCurrentTimeSeconds(fakeMillis) == 0u);
  assert(shot.getResetTime() == 100u);
  fakeMillis = 300u;
  shot.run(ws, nex);
  assert(shot.getTimerTime() == 200u);
  std::cout << "StopWatch wraparound and shot reset passed\n";
}

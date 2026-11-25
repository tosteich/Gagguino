#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include <limits>
#include "../LogFormatter.h"

int main() {
  sensorReadings records[30]{};
  for (int i = 0; i < 30; ++i) {
    records[i] = {95.125f, 9.5f, 100, 1764070000UL};
  }
  char buffer[30 * 96]{};
  assert(formatLogRecords(records, 30, buffer, sizeof(buffer)));
  const char* row = "95.125000,9.500000,100,1764070000\n";
  assert(strncmp(buffer, row, strlen(row)) == 0);
  assert(strlen(buffer) == 30 * strlen(row));

  // Former 1200-byte buffer can be too small for extreme float values.
  for (auto& r : records) {
    r.Temp = std::numeric_limits<float>::max();
    r.Press = -std::numeric_limits<float>::max();
    r.BoilPw = INT_MIN;
    r.Time = ULONG_MAX;
  }
  struct Guarded {
    char buffer[1200];
    char guard[8];
  } guarded{};
  memset(guarded.guard, 0x5a, sizeof(guarded.guard));
  assert(!formatLogRecords(records, 30, guarded.buffer, sizeof(guarded.buffer)));
  assert(guarded.buffer[0] == '\0');
  for (char c : guarded.guard) assert(c == 0x5a);
  assert(formatLogRecords(records, 30, buffer, sizeof(buffer)));

  char tiny[1] = {42};
  assert(!formatLogRecords(records, 1, tiny, sizeof(tiny)));
  assert(tiny[0] == '\0');
  assert(!formatLogRecords(records, 1, nullptr, 0));
  assert(!formatLogRecords(nullptr, 1, buffer, sizeof(buffer)));
  assert(formatLogRecords(records, 0, buffer, sizeof(buffer)));
  assert(buffer[0] == '\0');
  std::cout << "FTP log formatter bounds tests passed\n";
}

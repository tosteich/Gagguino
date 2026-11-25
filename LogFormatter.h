#pragma once
#include <stddef.h>
#include <stdio.h>

// A single CSV row preserves the original columns and formatting.
typedef struct {
  float Temp;
  float Press;
  int BoilPw;
  unsigned long Time;
} sensorReadings;

// Never write past capacity. The caller must not upload a partial record set.
inline bool formatLogRecords(const sensorReadings* records, size_t count,
                             char* output, size_t capacity) {
  if (!records || !output || capacity == 0) return false;
  size_t used = 0;
  output[0] = '\0';
  for (size_t i = 0; i < count; ++i) {
    const size_t remaining = capacity - used;
    const int written = snprintf(output + used, remaining, "%f,%f,%d,%lu\n",
                                 static_cast<double>(records[i].Temp),
                                 static_cast<double>(records[i].Press),
                                 records[i].BoilPw, records[i].Time);
    if (written < 0 || static_cast<size_t>(written) >= remaining) {
      output[0] = '\0'; // Prevent accidental upload of truncated data.
      return false;
    }
    used += static_cast<size_t>(written);
  }
  return true;
}

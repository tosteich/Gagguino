// g++ -std=c++17 -Wall -Wextra -Werror tests/manometer_sensor_fault_test.cpp -o /tmp/manometer_test && /tmp/manometer_test
#include <cassert>
#include <cmath>
#include <iostream>

static int adc = 0;
static int samples[32] = {};
static int nextSample = 0;
static bool useSequence = false;
constexpr int PRESSURE_FAULT_TEST_PIN = 33;
int analogRead(int pin) {
  assert(pin == PRESSURE_FAULT_TEST_PIN);
  return useSequence ? samples[(nextSample++) % 32] : adc;
}
#include "../Manometer.h"

int main() {
  Manometer sensor;
  assert(!sensor.isValid()); // no sample yet
  adc = 0;
  assert(sensor.getPressure() == 0.0f);
  assert(!sensor.isValid());
  adc = 4095;
  assert(sensor.getPressure() == 0.0f);
  assert(!sensor.isValid());
  adc = 32;
  sensor.getPressure();
  assert(!sensor.isValid());
  adc = 4063;
  sensor.getPressure();
  assert(!sensor.isValid());
  // A legitimate zero-pressure sensor voltage is above the ADC rail.
  adc = 240;
  assert(sensor.getPressure() >= 0.0f);
  assert(sensor.isValid());
  adc = 3000;
  assert(sensor.getPressure() > 0.0f);
  assert(sensor.isValid());
  adc = 0;
  sensor.getPressure();
  assert(!sensor.isValid());
  adc = 240;
  sensor.getPressure();
  assert(sensor.isValid()); // Recovery after valid data returns.
  // A 50/50 mix of both ADC rails averages to a plausible midrange,
  // but must never be accepted as usable PID feedback.
  useSequence = true;
  for (int i = 0; i < 32; ++i) samples[i] = i % 2 ? 4095 : 0;
  nextSample = 0;
  assert(sensor.getPressure() == 0.0f);
  assert(!sensor.isValid());

  // Reject four rail samples even if all other readings are plausible.
  for (int i = 0; i < 32; ++i) samples[i] = 2000;
  for (int i = 0; i < 4; ++i) samples[i] = 0;
  nextSample = 0;
  assert(sensor.getPressure() == 0.0f);
  assert(!sensor.isValid());

  // Do not react to a single transient sample at the rail.
  for (int i = 0; i < 32; ++i) samples[i] = 2000;
  samples[0] = 4095;
  nextSample = 0;
  assert(sensor.getPressure() > 0.0f);
  assert(sensor.isValid());

  // A fully normal sample block clears a prior fault.
  for (int i = 0; i < 32; ++i) samples[i] = 240;
  nextSample = 0;
  sensor.getPressure();
  assert(sensor.isValid());
  std::cout << "Manometer rail-fault classification tests passed\n";
}

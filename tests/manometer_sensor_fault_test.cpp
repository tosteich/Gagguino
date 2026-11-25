// g++ -std=c++17 -Wall -Wextra -Werror tests/manometer_sensor_fault_test.cpp -o /tmp/manometer_test && /tmp/manometer_test
#include <cassert>
#include <cmath>
#include <iostream>

static int adc = 0;
constexpr int PRESSURE_FAULT_TEST_PIN = 33;
int analogRead(int pin) { assert(pin == PRESSURE_FAULT_TEST_PIN); return adc; }
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
  std::cout << "Manometer rail-fault classification tests passed\n";
}

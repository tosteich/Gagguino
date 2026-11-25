// USB-only PID diagnostic; no GPIO writes, no heater or pump operation.
// ESP32 Dev Module, Arduino-ESP32 core 3.3.12, Serial Monitor 115200.
// All scenarios use synthesized temperatures sampled every 200 ms.
#include <Arduino.h>
#include <math.h>
#include "GaggiaPid.h"

static constexpr int SENSOR_PERIOD_MS = 200;
static uint32_t passCount = 0;
static uint32_t failCount = 0;

void expect(const char* label, bool condition) {
  if (condition) ++passCount;
  else ++failCount;
  Serial.printf("CHECK %-27s %s pass=%lu fail=%lu\n",
    label, condition ? "PASS" : "FAIL",
    (unsigned long)passCount, (unsigned long)failCount);
}

void printStep(const char* phase, float celsius, int power) {
  Serial.printf("PID phase=%-14s input=%7.2fC power=%3d%%\n",
    phase, celsius, power);
}

void runStepSequence() {
  GaggiaPid pid(SENSOR_PERIOD_MS);
  // Illustrative coefficients, not tuned for a real coffee machine.
  pid.setupPid(2.0f, 0.5f, 0.0f, 95.0f);
  const float temperatures[] = {80, 85, 90, 94, 95, 96, 100};
  for (float temperature : temperatures) {
    const int power = pid.getResult(temperature);
    printStep("approach", temperature, power);
    expect("bounded_output", power >= 0 && power <= 100);
  }
}

void runSaturationRecovery() {
  GaggiaPid pid(SENSOR_PERIOD_MS);
  pid.setupPid(2.0f, 1.0f, 0.0f, 95.0f);
  for (int i = 0; i < 1000; ++i) {
    const int power = pid.getResult(25.0f);
    if (power < 0 || power > 100) {
      expect("saturated_output_bounded", false);
      return;
    }
  }
  const int at100 = pid.getResult(100.0f);
  printStep("after_saturation", 100.0f, at100);
  expect("saturation_recovery_bounded", at100 >= 0 && at100 <= 100);

  // Normal integration must recover following a sustained hot condition.
  for (int i = 0; i < 1000; ++i) pid.getResult(110.0f);
  const int afterHot = pid.getResult(110.0f);
  printStep("hot_sustained", 110.0f, afterHot);
  expect("sustained_hot_zero_output", afterHot == 0);
}

void runTrustLossAndModeChange() {
  GaggiaPid pid(SENSOR_PERIOD_MS);
  pid.setupPid(2.0f, 1.0f, 0.0f, 95.0f);
  for (int i = 0; i < 50; ++i) pid.getResult(80.0f);

  // Simulate application-level stale-sensor shutdown and state reset.
  pid.resetState();
  const int recovered = pid.getResult(94.0f);
  printStep("sensor_recovered", 94.0f, recovered);
  expect("recovered_pid_no_old_integral", recovered <= 3);

  // Steam -> brew transition: reset state before new setpoint.
  pid.setSetPoint(110.0f);
  for (int i = 0; i < 50; ++i) pid.getResult(90.0f);
  pid.resetState();
  pid.setSetPoint(93.0f);
  const int brew = pid.getResult(100.0f);
  printStep("steam_to_brew", 100.0f, brew);
  expect("steam_to_brew_no_heat", brew == 0);
}

void runInvalidMeasurement() {
  GaggiaPid pid(SENSOR_PERIOD_MS);
  pid.setupPid(2.0f, 1.0f, 0.0f, 95.0f);
  pid.getResult(90.0f);
  expect("nan_input_zero_output", pid.getResult(NAN) == 0);
  expect("infinite_input_zero_output", pid.getResult(INFINITY) == 0);
}

void runDerivativeObservation() {
  GaggiaPid pid(SENSOR_PERIOD_MS);
  pid.setupPid(0.0f, 0.0f, 1.0f, 95.0f);
  const int initial = pid.getResult(90.0f);
  const int rising = pid.getResult(91.0f);
  const int falling = pid.getResult(90.0f);
  Serial.printf("PID DERIVATIVE first=%d rising_1C=%d falling_1C=%d\n",
    initial, rising, falling);
  // The pump must keep legacy derivative semantics.
  expect("pump_legacy_derivative", initial == 0 && rising == 5 && falling == 0);

  // The boiler must oppose a rising measurement and support heat on cooling.
  GaggiaPid boiler(SENSOR_PERIOD_MS);
  boiler.setDerivativeOpposesMeasurement(true);
  boiler.setupPid(0.0f, 0.0f, 1.0f, 95.0f);
  const int boilerFirst = boiler.getResult(90.0f);
  const int boilerRising = boiler.getResult(91.0f);
  const int boilerFalling = boiler.getResult(90.0f);
  Serial.printf("BOILER DERIVATIVE first=%d rising_1C=%d falling_1C=%d\\n",
    boilerFirst, boilerRising, boilerFalling);
  expect("boiler_opposes_rising_temp",
    boilerFirst == 0 && boilerRising == 0 && boilerFalling == 5);

  // Compare signs with a nonzero proportional contribution, away from
  // output clipping, to ensure the difference is not hidden at zero.
  GaggiaPid comparison(SENSOR_PERIOD_MS);
  comparison.setDerivativeOpposesMeasurement(true);
  comparison.setupPid(2.0f, 0.0f, 1.0f, 95.0f);
  comparison.getResult(90.0f);
  const int risingWithP = comparison.getResult(91.0f);
  const int fallingWithP = comparison.getResult(90.0f);
  Serial.printf("BOILER P+D rising=%d falling=%d\\n",
    risingWithP, fallingWithP);
  expect("boiler_damping_with_P", risingWithP == 3 && fallingWithP == 15);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("PID SYNTHETIC DIAGNOSTIC - NO GPIO OUTPUTS");
  Serial.println("This does not test a real LMT01, heater, boiler or thermal plant.");
  runStepSequence();
  runSaturationRecovery();
  runTrustLossAndModeChange();
  runInvalidMeasurement();
  runDerivativeObservation();
  Serial.printf("PID_DIAGNOSTIC_DONE pass=%lu fail=%lu\n",
    (unsigned long)passCount, (unsigned long)failCount);
}
void loop() { delay(1000); }

// USB-only test: never connect GPIO14 to relay, heater or mains.
// Arduino ESP32 Dev Module, Arduino-ESP32 3.3.12, Serial Monitor 115200.
// This standalone sketch uses a synchronized local copy of production Boiler.h.
#include <Arduino.h>
#include "Boiler.h"

static Boiler boiler(1000);
static uint32_t cycles = 0;
static uint32_t passed = 0;
static uint32_t failed = 0;

bool gpioHigh() {
  return digitalRead(HEAT_RELAY_PIN) == HIGH;
}

void checkResult(const char* name, bool ok) {
  if (ok) ++passed;
  else ++failed;
  Serial.printf("CHECK %-24s %s pass=%lu fail=%lu\n",
                name, ok ? "PASS" : "FAIL",
                (unsigned long)passed, (unsigned long)failed);
}

void serviceFor(uint32_t durationMs) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < durationMs) {
    boiler.updateHeater();
    delay(1);
  }
}

// Measure logical GPIO HIGH duration over 5 PWM cycles (5 seconds).
// This is an estimate based on repeated digitalRead(), not an oscilloscope.
void measureDuty(int requestedPercent) {
  boiler.setHeatPower(0);
  boiler.updateHeater();
  delay(20);
  checkResult("OFF_BEFORE_DUTY", !gpioHigh());

  boiler.setHeatPower(requestedPercent);
  const uint32_t start = millis();
  uint32_t previous = start;
  uint32_t highMs = 0;
  uint32_t transitions = 0;
  bool previousHigh = gpioHigh();
  while ((uint32_t)(millis() - start) < 5000) {
    boiler.updateHeater();
    const uint32_t now = millis();
    const bool high = gpioHigh();
    const uint32_t elapsed = now - previous;
    if (previousHigh) highMs += elapsed;
    if (high != previousHigh) ++transitions;
    previous = now;
    previousHigh = high;
    delay(1);
  }
  const uint32_t total = (uint32_t)(millis() - start);
  // Flush the last sample with its last observed state.
  if (previousHigh) highMs += (uint32_t)(millis() - previous);
  const float actual = total ? 100.0f * highMs / total : 0.0f;
  const float difference = actual > requestedPercent
      ? actual - requestedPercent : requestedPercent - actual;
  bool ok = difference <= 5.0f;
  if (requestedPercent == 0) ok = highMs == 0;
  if (requestedPercent == 100) ok = highMs >= total - 5;
  Serial.printf("DUTY requested=%d measured=%.1f%% highMs=%lu totalMs=%lu transitions=%lu %s\n",
                requestedPercent, actual, (unsigned long)highMs,
                (unsigned long)total, (unsigned long)transitions,
                ok ? "PASS" : "FAIL");
  if (ok) ++passed;
  else ++failed;

  // Simulate an emergency trust loss: command OFF while control was active.
  boiler.setHeatPower(0);
  checkResult("CUTOFF_IMMEDIATE", !gpioHigh());
  serviceFor(1200);
  checkResult("CUTOFF_REMAINS_OFF", !gpioHigh());
}

void setup() {
  // Important: this operation happens only after firmware startup.
  boiler.begin();
  Serial.begin(115200);
  delay(300);
  Serial.println("BOILER DUTY DIAGNOSTIC - USB ONLY");
  Serial.println("GPIO14 WILL BE HIGH. KEEP GPIO14 PHYSICALLY DISCONNECTED.");
  Serial.println("NO RELAY / HEATER / PUMP / MAINS / MACHINE WIRING.");
}

void loop() {
  ++cycles;
  Serial.printf("BEGIN cycle=%lu\n", (unsigned long)cycles);
  measureDuty(0);
  measureDuty(10);
  measureDuty(50);
  measureDuty(90);
  measureDuty(100);

  boiler.setHeatPower(100);
  checkResult("FULL_ON_BEFORE_PERIOD0", gpioHigh());
  boiler.setPeriod(0);
  checkResult("ZERO_PERIOD_FORCE_OFF", !gpioHigh());
  boiler.setHeatPower(100);
  checkResult("ZERO_PERIOD_STAYS_OFF", !gpioHigh());
  boiler.setPeriod(1000);
  boiler.setHeatPower(0);
  checkResult("RESTORED_PERIOD_OFF", !gpioHigh());

  Serial.printf("END cycle=%lu pass=%lu fail=%lu\n",
                (unsigned long)cycles, (unsigned long)passed, (unsigned long)failed);
  delay(1500);
}

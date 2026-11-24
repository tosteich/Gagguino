// Full-firmware USB-only temperature/boiler integration diagnostic.
// Enable only in BenchTestConfig.h. GPIO23 -> GPIO25 on separate bare ESP32.
// GPIO14, GPIO18, GPIO19 and all other pins MUST remain unconnected.
#ifdef GAGGUINO_LMT_BENCH_TEST
#include <Arduino.h>
constexpr uint8_t BENCH_GENERATOR_PIN = 23;
constexpr uint32_t BENCH_CYCLE_MS = 10000;
constexpr uint32_t BENCH_ACTIVE_MS = 6000;
constexpr int BENCH_LMT_PULSES = 1200; // ~25 C
volatile bool benchPulseActive = false;

void benchGeneratorTask(void*) {
  const uint32_t epoch = millis();
  for (;;) {
    const bool active = ((uint32_t)(millis() - epoch) % BENCH_CYCLE_MS) < BENCH_ACTIVE_MS;
    benchPulseActive = active;
    digitalWrite(BENCH_GENERATOR_PIN, LOW);
    if (active) {
      for (int i = 0; i < BENCH_LMT_PULSES; ++i) {
        digitalWrite(BENCH_GENERATOR_PIN, HIGH);
        delayMicroseconds(40);
        digitalWrite(BENCH_GENERATOR_PIN, LOW);
        delayMicroseconds(40);
      }
      vTaskDelay(pdMS_TO_TICKS(200));
    } else {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

void setupBenchLmtSimulation() {
  pinMode(BENCH_GENERATOR_PIN, OUTPUT);
  digitalWrite(BENCH_GENERATOR_PIN, LOW);
  Serial.println("BENCH MODE: LMT pulse simulation; fixed 100% heater request");
  Serial.println("GPIO14 WILL GO HIGH; ALL outputs MUST be physically unconnected");
  BaseType_t ok = xTaskCreatePinnedToCore(benchGeneratorTask, "lmtbench", 3072, nullptr, 1, nullptr, 0);
  if (ok != pdPASS) {
    Serial.println("BENCH ERROR: cannot create generator task");
    ESP.restart();
  }
}

void reportBenchLmtSimulation() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastMs) < 1000) return;
  lastMs = now;
  Serial.printf("BENCH generator=%s trusted=%d temp=%.2f heatPower=%d gpio14=%s raw=%d prev=%d maxRaw=%d frames=%lu accepted=%lu rejected=%lu errors=%lu reads=%lu\n",
                benchPulseActive ? "ACTIVE" : "SILENT",
                temperatureGuard.fresh(now) ? 1 : 0, currentTemperature, heatPower,
                digitalRead(HEAT_RELAY_PIN) == HIGH ? "HIGH" : "LOW",
                pulseCounter, oldPulseCount, benchLargestCount,
                (unsigned long)benchCompleteFrames,
                (unsigned long)benchAcceptedFrames,
                (unsigned long)benchRejectedFrames,
                (unsigned long)benchPcntErrors,
                (unsigned long)benchPcntReads);
  if ((uint32_t)(now - lastMs) < 5) {
    // Snapshot of most recently completed frame; useful when maxRaw stays below 800.
    Serial.printf("BENCH lastFrame=%d\n", benchLastFrameCount);
  }
}
#endif

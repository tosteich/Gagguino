// Full-firmware USB-only temperature/boiler integration diagnostic.
// Enable only in BenchTestConfig.h. GPIO23 -> GPIO25 on separate bare ESP32.
// GPIO14, GPIO18, GPIO19 and all other pins MUST remain unconnected.
#ifdef GAGGUINO_LMT_BENCH_TEST
#include <Arduino.h>
// Arduino concatenates .ino files alphabetically. pulseCounter.ino is later,
// so diagnostic globals defined there need forward declarations here.
extern int pulseCounter;
extern int oldPulseCount;
extern uint32_t benchPcntReads;
extern uint32_t benchCompleteFrames;
extern uint32_t benchAcceptedFrames;
extern uint32_t benchRejectedFrames;
extern uint32_t benchPcntErrors;
extern int benchLargestCount;
extern int benchLastFrameCount;
constexpr uint8_t BENCH_GENERATOR_PIN = 23;
constexpr uint32_t BENCH_CYCLE_MS = 20000; // 10s forced LOW + 6s pulses + 4s silence
constexpr uint32_t BENCH_ACTIVE_MS = 6000;
constexpr int BENCH_LMT_PULSES = 1200; // ~25 C
constexpr uint32_t BENCH_PULSE_HZ = 10000;
constexpr uint32_t BENCH_BURST_MS = 120;
volatile uint32_t benchGeneratorBursts = 0;
volatile bool benchPulseActive = false;
volatile bool benchForcedLow = true;

void benchGeneratorTask(void*) {
  const uint32_t epoch = millis();
  for (;;) {
    const uint32_t phase = (uint32_t)(millis() - epoch) % BENCH_CYCLE_MS;
    const bool forcedLow = phase < 10000;
    const bool active = phase >= 10000 && phase < 10000 + BENCH_ACTIVE_MS;
    benchForcedLow = forcedLow;
    benchPulseActive = active;
    if (active) {
      // Hardware-timed pulse edges independent of FreeRTOS scheduling.
      // Burst stop is scheduled in software: total count is approximate.
      ledcWrite(BENCH_GENERATOR_PIN, 128);
      vTaskDelay(pdMS_TO_TICKS(BENCH_BURST_MS));
      ledcWrite(BENCH_GENERATOR_PIN, 0);
      ++benchGeneratorBursts;
      vTaskDelay(pdMS_TO_TICKS(200));
    } else {
      ledcWrite(BENCH_GENERATOR_PIN, 0);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

void setupBenchLmtSimulation() {
  if (!ledcAttach(BENCH_GENERATOR_PIN, BENCH_PULSE_HZ, 8)) {
    Serial.println("BENCH ERROR: LEDC attach failed; stop test");
    while (true) delay(1000);
  }
  ledcWrite(BENCH_GENERATOR_PIN, 0);
  Serial.println("BENCH MODE: 10s FORCED_LOW / 6s ACTIVE / 4s SILENT; LEDC 10kHz");
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
  Serial.printf("BENCH generator=%s pin23=%d pin25=%d trusted=%d temp=%.2f heatPower=%d gpio14=%s raw=%d prev=%d maxRaw=%d frames=%lu accepted=%lu rejected=%lu errors=%lu reads=%lu genBursts=%lu\n",
                benchForcedLow ? "FORCED_LOW" : (benchPulseActive ? "ACTIVE" : "SILENT"),
                digitalRead(BENCH_GENERATOR_PIN), digitalRead(LMT_PIN),
                temperatureGuard.fresh(now) ? 1 : 0, currentTemperature, heatPower,
                digitalRead(HEAT_RELAY_PIN) == HIGH ? "HIGH" : "LOW",
                pulseCounter, oldPulseCount, benchLargestCount,
                (unsigned long)benchCompleteFrames,
                (unsigned long)benchAcceptedFrames,
                (unsigned long)benchRejectedFrames,
                (unsigned long)benchPcntErrors,
                (unsigned long)benchPcntReads,
                (unsigned long)benchGeneratorBursts);
  Serial.printf("BENCH lastFrame=%d\n", benchLastFrameCount);
}
#endif

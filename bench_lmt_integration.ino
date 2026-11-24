// Full-firmware USB-only temperature/boiler integration diagnostic.
// Enable only in BenchTestConfig.h. GPIO23 -> GPIO25 on separate bare ESP32.
// GPIO14, GPIO18, GPIO19 and all other pins MUST remain unconnected.
#ifdef GAGGUINO_LMT_BENCH_TEST
#include <Arduino.h>
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "driver/gpio.h"
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
extern int benchLastRejectedFrameCount;
constexpr uint8_t BENCH_GENERATOR_PIN = 23;
// Switch to true only when isolating the GPIO line without pulse generation.
constexpr bool BENCH_LOW_ONLY = false;
constexpr uint32_t BENCH_CYCLE_MS = 20000; // 10s forced LOW + 6s pulses + 4s silence
constexpr uint32_t BENCH_ACTIVE_MS = 6000;
constexpr int BENCH_LMT_PULSES = 1200; // ~25 C
constexpr uint32_t BENCH_PULSE_HZ = 10000;
constexpr uint32_t BENCH_BURST_MS = 120;
volatile uint32_t benchGeneratorBursts = 0;
volatile bool benchPulseActive = false;
volatile bool benchForcedLow = true;
volatile uint32_t benchLowWrites = 0;
volatile uint32_t benchImmediateHigh = 0;

// Force the wire to a real GPIO LOW state between hardware pulse bursts.
// ledcWrite(pin, 0) alone was NOT observed as LOW on this ESP32.
void benchForceOutputLow() {
  pinMode(BENCH_GENERATOR_PIN, OUTPUT);
  digitalWrite(BENCH_GENERATOR_PIN, LOW);
  ++benchLowWrites;
  if (digitalRead(BENCH_GENERATOR_PIN) != LOW) ++benchImmediateHigh;
}

void benchGeneratorTask(void*) {
  const uint32_t epoch = millis();
  for (;;) {
    if (BENCH_LOW_ONLY) {
      benchForcedLow = true;
      benchPulseActive = false;
      benchForceOutputLow();
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    const uint32_t phase = (uint32_t)(millis() - epoch) % BENCH_CYCLE_MS;
    const bool forcedLow = phase < 10000;
    const bool active = phase >= 10000 && phase < 10000 + BENCH_ACTIVE_MS;
    benchForcedLow = forcedLow;
    benchPulseActive = active;
    if (active) {
      // Attach LEDC only during a burst, then detach and force GPIO LOW.
      if (!ledcAttach(BENCH_GENERATOR_PIN, BENCH_PULSE_HZ, 8)) {
        Serial.println("BENCH ERROR: LEDC attach failed");
        benchForceOutputLow();
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      if (!ledcWrite(BENCH_GENERATOR_PIN, 128)) {
        Serial.println("BENCH ERROR: LEDC write failed");
      }
      vTaskDelay(pdMS_TO_TICKS(BENCH_BURST_MS));
      ledcWrite(BENCH_GENERATOR_PIN, 0);
      ledcDetach(BENCH_GENERATOR_PIN);
      benchForceOutputLow();
      ++benchGeneratorBursts;
      vTaskDelay(pdMS_TO_TICKS(500)); // bench-only longer separation
    } else {
      // LEDC is detached outside bursts; do not rely on duty=0.
      benchForceOutputLow();
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

void setupBenchLmtSimulation() {
  // UART2 is mapped to GPIO16/17 in setup(). Do not touch PCNT GPIO25
  // output enable or pull resistors here; test the normal input setup.
  benchForceOutputLow();
  Serial.println(BENCH_LOW_ONLY
      ? "BENCH MODE: LOW_ONLY; LEDC never attached; GPIO23 forced LOW"
      : "BENCH MODE: 10s FORCED_LOW / 6s ACTIVE / 4s SILENT; LEDC 10kHz");
  Serial.println("BENCH MODE: LEDC detached and GPIO23 driven LOW between bursts");
  Serial.println("GPIO14 WILL GO HIGH; ALL outputs MUST be physically unconnected");
  BaseType_t ok = xTaskCreatePinnedToCore(benchGeneratorTask, "lmtbench", 3072, nullptr, 1, nullptr, 0);
  if (ok != pdPASS) {
    Serial.println("BENCH ERROR: cannot create generator task");
    benchForceOutputLow();
    while (true) delay(1000);
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
  const uint32_t pin23Mask = (1UL << BENCH_GENERATOR_PIN);
  const uint32_t pin25Mask = (1UL << LMT_PIN);
  const uint32_t outEn = REG_READ(GPIO_ENABLE_REG);
  const uint32_t out = REG_READ(GPIO_OUT_REG);
  const uint32_t in = REG_READ(GPIO_IN_REG);
  Serial.printf("BENCH GPIO hw_en23=%u hw_out23=%u hw_in23=%u hw_in25=%u lowWrites=%lu immediateHigh=%lu lastFrame=%d lastRejected=%d\n",
                (outEn & pin23Mask) ? 1 : 0, (out & pin23Mask) ? 1 : 0,
                (in & pin23Mask) ? 1 : 0, (in & pin25Mask) ? 1 : 0,
                (unsigned long)benchLowWrites, (unsigned long)benchImmediateHigh,
                benchLastFrameCount, benchLastRejectedFrameCount);
  static uint32_t lastDump = 0;
  if ((uint32_t)(now - lastDump) >= 10000) {
    lastDump = now;
    Serial.println("BENCH IO MUX / GPIO matrix diagnostic:");
    gpio_dump_io_configuration(stdout, (1ULL << BENCH_GENERATOR_PIN) | (1ULL << LMT_PIN));
  }
}
#endif

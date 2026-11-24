// Standalone, USB-only LMT01 -> TemperatureGuard -> Boiler shutdown test.
// ESP32 Dev Module, Arduino-ESP32 Core 3.3.12, Serial Monitor 115200.
// GPIO23 -> GPIO25, on the same bare test ESP32; GPIO14 UNCONNECTED.
// Never connect relay, boiler, mains, or actual coffee-machine wiring.
#include <Arduino.h>
#include "driver/pulse_cnt.h"
#include "TemperatureGuard.h"
#include "Boiler.h"

constexpr int GENERATOR_PIN = 23;
constexpr int SENSOR_PIN = 25;
constexpr int FIXED_HEAT_REQUEST = 100; // intentionally nonzero; GPIO14 must be UNCONNECTED
constexpr int NORMAL_PULSES = 1200; // 25 C
constexpr int INVALID_PULSES = 100; // -43.75 C, outside accepted range
constexpr uint32_t POLL_PERIOD_MS = 20;
pcnt_unit_handle_t unit = nullptr;
pcnt_channel_handle_t channel = nullptr;
TemperatureGuard guard;
Boiler boiler(1000);  // Same Boiler class as production firmware
unsigned long totalPass = 0;
unsigned long totalFail = 0;

void initCounter() {
  pcnt_unit_config_t cfg = {};
  cfg.low_limit = -1;
  cfg.high_limit = 32767;
  ESP_ERROR_CHECK(pcnt_new_unit(&cfg, &unit));
  pcnt_chan_config_t ch = {};
  ch.edge_gpio_num = SENSOR_PIN;
  ch.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(unit, &ch, &channel));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(channel,
       PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(channel,
       PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
  pcnt_glitch_filter_config_t filter = {};
  filter.max_glitch_ns = 1250;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filter));
  ESP_ERROR_CHECK(pcnt_unit_enable(unit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
  ESP_ERROR_CHECK(pcnt_unit_start(unit));
}

bool applySafetyAndRead() {
  const bool trusted = guard.fresh(millis());
  boiler.setHeatPower(trusted ? FIXED_HEAT_REQUEST : 0);
  boiler.updateHeater();
  return digitalRead(HEAT_RELAY_PIN) == HIGH;
}

void check(const char* name, bool expectedTrusted, bool expectedHigh) {
  const bool trusted = guard.fresh(millis());
  const bool high = applySafetyAndRead();
  const bool ok = trusted == expectedTrusted && high == expectedHigh;
  if (ok) ++totalPass;
  else ++totalFail;
  Serial.printf("%s trusted=%d gpio14=%s expected=%s %s\n",
                name, trusted ? 1 : 0, high ? "HIGH" : "LOW",
                expectedHigh ? "HIGH" : "LOW", ok ? "PASS" : "FAIL");
}

void sendBurst(int pulses) {
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
  for (int i=0; i<pulses; ++i) {
    digitalWrite(GENERATOR_PIN, HIGH);
    delayMicroseconds(40);
    digitalWrite(GENERATOR_PIN, LOW);
    delayMicroseconds(40);
  }
  // Require two stable readings before accepting a complete frame.
  delay(5);
  int first = -1, second = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(unit, &first));
  delay(5);
  ESP_ERROR_CHECK(pcnt_unit_get_count(unit, &second));
  if (first != second) {
    Serial.printf("ERROR unstable PCNT count %d -> %d\n",first,second);
    ++totalFail;
    guard.reset();
  } else {
    const bool accepted = guard.accept(second, millis());
    Serial.printf("FRAME sent=%d read=%d accepted=%d temp=%.2f\n",
                  pulses, second, accepted ? 1 : 0, second / 16.0f - 50.0f);
    if (second != pulses) {
      Serial.println("ERROR unexpected PCNT count");
      ++totalFail;
      guard.reset();
    }
  }
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
}

void waitAndService(uint32_t ms) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ms) {
    applySafetyAndRead(); // keeps the output safe when the data expire
    delay(POLL_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  // Boiler() constructor also initializes GPIO14 LOW, but set explicitly here.
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  digitalWrite(HEAT_RELAY_PIN, LOW);
  pinMode(GENERATOR_PIN, OUTPUT);
  digitalWrite(GENERATOR_PIN, LOW);
  pinMode(SENSOR_PIN, INPUT);
  initCounter();
  Serial.println("HEATER FAIL-SAFE DIAGNOSTIC; GPIO14 MUST BE UNCONNECTED");
  Serial.println("GPIO23 -> GPIO25 jumper only; USB-powered separate ESP32");
}

void loop() {
  static uint32_t cycle = 0;
  ++cycle;
  guard.reset();
  boiler.setHeatPower(0);
  Serial.printf("BEGIN cycle=%lu\n", (unsigned long)cycle);
  check("NO_SENSOR", false, false);

  sendBurst(NORMAL_PULSES);
  check("FIRST_FRAME", false, false);
  waitAndService(100);
  sendBurst(NORMAL_PULSES);
  check("SECOND_FRAME", true, true);

  waitAndService(400);
  check("STILL_FRESH", true, true);

  waitAndService(900); // total > 1000 ms since last qualified burst
  check("SENSOR_LOST", false, false);

  sendBurst(NORMAL_PULSES);
  check("RECOVERY_FIRST", false, false);
  waitAndService(100);
  sendBurst(NORMAL_PULSES);
  check("RECOVERY_SECOND", true, true);

  sendBurst(INVALID_PULSES);
  check("INVALID_FRAME", false, false);

  Serial.printf("END cycle=%lu pass=%lu fail=%lu\n",
                (unsigned long)cycle, totalPass, totalFail);
  waitAndService(1500);
}

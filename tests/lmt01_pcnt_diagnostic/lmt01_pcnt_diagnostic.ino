// Standalone PCNT/LMT01 count test for Arduino-ESP32 3.3.12.
// Open this sketch alone (NOT together with Gagguino).
// With USB disconnected, wire GPIO23 -> GPIO25 using ONE insulated jumper.
// Test board only: NO sensor, boiler, pump, mains, dimmer, or other peripherals.
// Serial Monitor: 115200 baud.
#include <Arduino.h>
#include "driver/pulse_cnt.h"

constexpr int GENERATOR_PIN = 23;
constexpr int LMT_INPUT_PIN = 25;
constexpr uint16_t TEST_PULSES[] = {800, 960, 1120, 1200, 1440, 1600};
constexpr size_t TEST_COUNT = sizeof(TEST_PULSES) / sizeof(TEST_PULSES[0]);
constexpr uint32_t HALF_PULSE_US = 40;
constexpr uint32_t COUNTER_POLL_MS = 5;

pcnt_unit_handle_t pcntUnit = nullptr;
pcnt_channel_handle_t pcntChannel = nullptr;

void setupCounter() {
  pcnt_unit_config_t config = {};
  config.low_limit = -1;
  config.high_limit = 32767;
  ESP_ERROR_CHECK(pcnt_new_unit(&config, &pcntUnit));

  pcnt_chan_config_t chanConfig = {};
  chanConfig.edge_gpio_num = LMT_INPUT_PIN;
  chanConfig.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(pcntUnit, &chanConfig, &pcntChannel));

  // Identical to the draft firmware: count the FALLING edge only.
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      pcntChannel, PCNT_CHANNEL_EDGE_ACTION_HOLD,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(
      pcntChannel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
      PCNT_CHANNEL_LEVEL_ACTION_KEEP));

  pcnt_glitch_filter_config_t filter = {};
  filter.max_glitch_ns = 1250;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcntUnit, &filter));
  ESP_ERROR_CHECK(pcnt_unit_enable(pcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(pcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_start(pcntUnit));
}

void emitPulses(uint16_t count) {
  for (uint16_t i = 0; i < count; ++i) {
    digitalWrite(GENERATOR_PIN, HIGH);
    delayMicroseconds(HALF_PULSE_US);
    digitalWrite(GENERATOR_PIN, LOW); // counted falling edge
    delayMicroseconds(HALF_PULSE_US);
  }
}

void runOneTest(uint16_t expected) {
  // Explicitly clear before each burst; the actual LMT firmware clears
  // when two consecutive samples (5 ms apart) are equal.
  ESP_ERROR_CHECK(pcnt_unit_clear_count(pcntUnit));
  delay(COUNTER_POLL_MS);
  int before = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(pcntUnit, &before));

  emitPulses(expected);
  delay(COUNTER_POLL_MS); // first read after the completed burst
  int count1 = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(pcntUnit, &count1));
  delay(COUNTER_POLL_MS); // second read, as in production LMT logic
  int count2 = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(pcntUnit, &count2));

  const bool stable = (count1 == count2);
  const bool counted = (count2 == expected && before == 0);
  const float temperature = count2 / 16.0f - 50.0f;

  ESP_ERROR_CHECK(pcnt_unit_clear_count(pcntUnit));
  delay(COUNTER_POLL_MS);
  int after = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(pcntUnit, &after));

  const bool pass = stable && counted && after == 0;
  Serial.printf("TEST pulses=%u counted=%d temp=%.4f before=%d stable=%d afterClear=%d %s\n",
                expected, count2, temperature, before, stable ? 1 : 0,
                after, pass ? "PASS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  pinMode(GENERATOR_PIN, OUTPUT);
  digitalWrite(GENERATOR_PIN, LOW);
  pinMode(LMT_INPUT_PIN, INPUT);
  setupCounter();
  Serial.println("LMT01 PCNT diagnostic, ESP32 Core 3.3.12");
  Serial.println("Wire GPIO23 to GPIO25 (3.3V logic only); GPIO25 must NOT float");
}

void loop() {
  static uint32_t cycle = 0;
  ++cycle;
  Serial.printf("BEGIN cycle=%lu\n", (unsigned long)cycle);
  for (size_t i = 0; i < TEST_COUNT; ++i) {
    runOneTest(TEST_PULSES[i]);
    delay(200);
  }
  Serial.printf("END cycle=%lu\n", (unsigned long)cycle);
  delay(1500);
}

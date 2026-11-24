// GPIO23 -> GPIO25 continuity and noise test. USB ONLY, separate bare ESP32.
// Arduino-ESP32 3.3.12, ESP32 Dev Module, 115200 baud.
// No heater, relay, pump, mains, or other hardware attached.
#include <Arduino.h>
#include "driver/pulse_cnt.h"
constexpr int OUT_PIN = 23;
constexpr int IN_PIN = 25;
pcnt_unit_handle_t unit = nullptr;
pcnt_channel_handle_t channel = nullptr;

void initPcnt() {
  pcnt_unit_config_t cfg = {};
  cfg.low_limit = -1;
  cfg.high_limit = 32767;
  ESP_ERROR_CHECK(pcnt_new_unit(&cfg, &unit));
  pcnt_chan_config_t ch = {};
  ch.edge_gpio_num = IN_PIN;
  ch.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(unit, &ch, &channel));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      channel, PCNT_CHANNEL_EDGE_ACTION_HOLD,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE)); // count falling edges
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(
      channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
      PCNT_CHANNEL_LEVEL_ACTION_KEEP));
  pcnt_glitch_filter_config_t filt = {};
  filt.max_glitch_ns = 1250;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filt));
  ESP_ERROR_CHECK(pcnt_unit_enable(unit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
  ESP_ERROR_CHECK(pcnt_unit_start(unit));
}

int pcntCount() {
  int n = -1;
  ESP_ERROR_CHECK(pcnt_unit_get_count(unit, &n));
  return n;
}

void holdLow(const char* name) {
  digitalWrite(OUT_PIN, LOW);
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
  Serial.printf("%s BEGIN in=%d count=%d\n", name, digitalRead(IN_PIN), pcntCount());
  for (int sec = 1; sec <= 5; ++sec) {
    delay(1000);
    Serial.printf("%s t=%ds in=%d count=%d\n", name, sec, digitalRead(IN_PIN), pcntCount());
  }
}

void oneHundredEdges() {
  digitalWrite(OUT_PIN, LOW);
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
  const int start = pcntCount();
  for (int i = 0; i < 100; ++i) {
    digitalWrite(OUT_PIN, HIGH);
    delay(5);
    digitalWrite(OUT_PIN, LOW); // falling edge
    delay(5);
  }
  delay(100);
  const int count = pcntCount();
  Serial.printf("PULSES expected=100 before=%d counted=%d input=%d %s\n",
                start, count, digitalRead(IN_PIN),
                count == 100 && start == 0 ? "PASS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  pinMode(OUT_PIN, OUTPUT);
  digitalWrite(OUT_PIN, LOW);
  pinMode(IN_PIN, INPUT);
  initPcnt();
  Serial.println("START GPIO23->GPIO25; USB-only; all other pins disconnected");
}

void loop() {
  holdLow("LOW_A");
  oneHundredEdges();
  holdLow("LOW_B");
  Serial.println("END CYCLE");
  delay(1000);
}

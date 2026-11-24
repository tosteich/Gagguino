#include "driver/pulse_cnt.h"

#define LMT_TIMER_INTERVAL 5000  // microseconds

static pcnt_unit_handle_t lmtPcntUnit = nullptr;
static pcnt_channel_handle_t lmtPcntChannel = nullptr;

int pulseCounter = 0;
int oldPulseCount = 0;

void initPulseCounter() {
  pcnt_unit_config_t config = {};
  config.low_limit = -1;
  config.high_limit = 32767;
  ESP_ERROR_CHECK(pcnt_new_unit(&config, &lmtPcntUnit));

  pcnt_chan_config_t channelConfig = {};
  channelConfig.edge_gpio_num = LMT_PIN;
  channelConfig.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(lmtPcntUnit, &channelConfig, &lmtPcntChannel));

  // Preserve old PCNT_COUNT_INC on falling edges (neg_mode).
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      lmtPcntChannel, PCNT_CHANNEL_EDGE_ACTION_HOLD,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(
      lmtPcntChannel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
      PCNT_CHANNEL_LEVEL_ACTION_KEEP));

  // Previous hardware filter: 100 APB clock cycles at 80 MHz = 1250 ns.
  pcnt_glitch_filter_config_t filter = {};
  filter.max_glitch_ns = 1250;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(lmtPcntUnit, &filter));

  ESP_ERROR_CHECK(pcnt_unit_enable(lmtPcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(lmtPcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_start(lmtPcntUnit));
}

hw_timer_t* lmtTimer = nullptr;

void ARDUINO_ISR_ATTR onLmtTimer() {
  timeToCheckCounter = true;
}

void checkThermoPulsesCounter() {
  if (timeToCheckCounter) {
    timeToCheckCounter = false;
    checkCounter();
  }
}

void checkCounter() {
  int count = 0;
  if (pcnt_unit_get_count(lmtPcntUnit, &count) != ESP_OK) {
    return;
  }
  pulseCounter = count;
  if (pulseCounter > 0) {
    if (oldPulseCount == pulseCounter) {
      thermoLmt.setCurrentPulses(pulseCounter);
      ESP_ERROR_CHECK(pcnt_unit_clear_count(lmtPcntUnit));
      oldPulseCount = 0;
    } else {
      oldPulseCount = pulseCounter;
    }
  }
}

void initPulseCounterTimer() {
  lmtTimer = timerBegin(1000000); // 1 MHz: 1 tick = 1 us
  if (!lmtTimer) {
    Serial.println("ERROR: Cannot initialize LMT timer");
    return;
  }
  timerAttachInterrupt(lmtTimer, &onLmtTimer);
  timerAlarm(lmtTimer, LMT_TIMER_INTERVAL, true, 0);
}

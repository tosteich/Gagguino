#pragma once

#define HEAT_RELAY_PIN 14

class Boiler {
private:
  int heatCycles = 0;
  boolean flag = false;
  const int maxPower = 100;
  unsigned long activePeriod = 0;
  unsigned long tmr = 0;
  int period;

public:
  Boiler(int period) : period(period) {
    pinMode(HEAT_RELAY_PIN, OUTPUT);
    digitalWrite(HEAT_RELAY_PIN, LOW);
    tmr = millis();
  }

  void setPeriod(int period) {
    this->period = period;
    if (period <= 0) {
      heatCycles = 0;
      flag = false;
      toggleHeatElementOnOff(false);
    }
    activePeriod = (unsigned long)heatCycles * (period > 0 ? period : 0) / maxPower;
  }

  void setHeatPower(int power) {
    const int previousPower = heatCycles;
    heatCycles = constrain(power, 0, maxPower);

    if (period <= 0 || heatCycles == 0) {
      heatCycles = 0;
      flag = false;
      toggleHeatElementOnOff(false);
      return;
    }

    if (heatCycles == maxPower) {
      flag = true;
      toggleHeatElementOnOff(true);
      return;
    }

    activePeriod = (unsigned long)heatCycles * (unsigned long)period / maxPower;

    // Start a fresh PWM phase when leaving a steady OFF/ON state.
    if (previousPower == 0 || previousPower == maxPower) {
      tmr = millis();
    }
  }

  void updateHeater() {
    if (period <= 0 || heatCycles == 0) {
      flag = false;
      toggleHeatElementOnOff(false);
      return;
    }

    if (heatCycles == maxPower) {
      flag = true;
      toggleHeatElementOnOff(true);
      return;
    }

    const unsigned long now = millis();
    const unsigned long phaseDuration = flag ? activePeriod : ((unsigned long)period - activePeriod);
    if ((unsigned long)(now - tmr) >= phaseDuration) {
      tmr = now;
      flag = !flag;
      toggleHeatElementOnOff(flag);
    }
  }

  void toggleHeatElementOnOff(boolean toggle) {
    digitalWrite(HEAT_RELAY_PIN, toggle ? HIGH : LOW);
  }
};

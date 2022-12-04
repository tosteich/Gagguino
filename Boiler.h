#define HEAT_RELAY_PIN 14

class Boiler {
private:
  int heatCycles;
  boolean flag = false;
  int maxPower = 100;
  unsigned long activePeriod, tmr;
  int period;

public:
  Boiler(int period) {
    pinMode(HEAT_RELAY_PIN, OUTPUT);
    this->period = period;
  }

  void setPeriod(int period) {
    this->period = period;
  }

  void setHeatPower(int power) {
    heatCycles = power;
    if (heatCycles == 0) {
      toggleHeatElementOnOff(false);
    }
    if (heatCycles == maxPower) {
      toggleHeatElementOnOff(true);
    }
    activePeriod = heatCycles * period / maxPower;
  }

  void updateHeater() {
    if (heatCycles != 0 && heatCycles != maxPower) {
      if (millis() - tmr >= (flag ? activePeriod : (period - activePeriod))) {
        tmr = millis();
        flag = !flag;
        toggleHeatElementOnOff(flag);
      }
    }
  }

  void toggleHeatElementOnOff(boolean toggle) {
    digitalWrite(HEAT_RELAY_PIN, toggle);
  }
};

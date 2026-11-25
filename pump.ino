// ESP32 Arduino Core 3.3.12: GPTimer-backed one-shot phase firing.
// 5 MHz preserves the old 80 MHz / 16 timer resolution (0.2 us/tick).
static constexpr uint32_t PUMP_TIMER_HZ = 5000000;
static constexpr uint32_t PUMP_GATE_PULSE_US = 40;

hw_timer_t* pumpTimer = nullptr;
portMUX_TYPE pumpTimerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool pumpPulsePending = false;
// A 0% phase request still generates a late triac pulse. This separate
// inhibit flag prevents firing when a pressure-sensor rail fault is found.
volatile bool pumpPulsesEnabled = true;

void ARDUINO_ISR_ATTR onPumpTimer() {
  bool fire = false;
  portENTER_CRITICAL_ISR(&pumpTimerMux);
  fire = pumpPulsePending;
  pumpPulsePending = false; // One pulse at most for each zero-crossing event.
  portEXIT_CRITICAL_ISR(&pumpTimerMux);

  if (fire && pumpPulsesEnabled) {
    digitalWrite(PUMP_DIMMER_M1_PIN, HIGH);
    delayMicroseconds(PUMP_GATE_PULSE_US);
    digitalWrite(PUMP_DIMMER_M1_PIN, LOW);
  }
}

void ARDUINO_ISR_ATTR handleInterrupt() {
  // Preserve the original characteristic for values 0..100.
  int requestedPower = (int)pumpPower;
  if (requestedPower < 0) requestedPower = 0;
  if (requestedPower > 100) requestedPower = 100;
  const uint32_t delayTicks = (uint32_t)(49000 - 478.5 * requestedPower);

  portENTER_CRITICAL_ISR(&pumpTimerMux);
  pumpPulsePending = pumpPulsesEnabled;
  timerWrite(pumpTimer, 0);
  // No autoreload: loss of zero-cross signal stops further gate pulses.
  timerAlarm(pumpTimer, delayTicks, false, 0);
  portEXIT_CRITICAL_ISR(&pumpTimerMux);
}

void setupPump() {
  pinMode(PUMP_DIMMER_M2_PIN, INPUT_PULLUP);
  pinMode(PUMP_DIMMER_M1_PIN, OUTPUT);
  digitalWrite(PUMP_DIMMER_M1_PIN, LOW);

  pumpTimer = timerBegin(PUMP_TIMER_HZ);
  if (!pumpTimer) {
    Serial.println("ERROR: Cannot initialize pump phase timer");
    return;
  }
  timerAttachInterrupt(pumpTimer, &onPumpTimer);
  // Do not set an alarm before the first zero-crossing.
  attachInterrupt(digitalPinToInterrupt(PUMP_DIMMER_M2_PIN), handleInterrupt, RISING);
}

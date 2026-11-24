#include <Ticker.h>

Ticker blinker;

int power = 0;

hw_timer_t* pumpTimer = NULL;
portMUX_TYPE pumpTimerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onPumpTimer() {
  portENTER_CRITICAL_ISR(&pumpTimerMux);
    digitalWrite(PUMP_DIMMER_M1_PIN, HIGH);
    delayMicroseconds(40);
    digitalWrite(PUMP_DIMMER_M1_PIN, LOW);
    timerWrite(pumpTimer, 0);
    timerAlarmWrite(pumpTimer, 50000, true);
  portEXIT_CRITICAL_ISR(&pumpTimerMux);
}

void IRAM_ATTR handleInterrupt() 
{
    portENTER_CRITICAL_ISR(&pumpTimerMux);
    // Do not allow invalid PID output to produce out-of-half-cycle alarms.
    const int requestedPower = constrain((int)pumpPower, 0, 100);
    power = 49000 - 478.5 * requestedPower;
    timerWrite(pumpTimer, 0);
    timerAlarmWrite(pumpTimer, power, true);
    
    portEXIT_CRITICAL_ISR(&pumpTimerMux);
}

void setupPump()
{
  pinMode(PUMP_DIMMER_M2_PIN, INPUT_PULLUP);
  pinMode(PUMP_DIMMER_M1_PIN, OUTPUT);
  

  pumpTimer = timerBegin(3, 16, true);
  timerAttachInterrupt(pumpTimer, &onPumpTimer, true);
  timerAlarmWrite(pumpTimer, 50000, true);
  timerAlarmEnable(pumpTimer);

  // Attach the zero-cross interrupt only after its timer is initialized.
  attachInterrupt(digitalPinToInterrupt(PUMP_DIMMER_M2_PIN), handleInterrupt, RISING);
}

#include <Ticker.h>

Ticker blinker;

int power = 0;

void ICACHE_RAM_ATTR handleInterrupt();

hw_timer_t* pumpTimer = NULL;
portMUX_TYPE pumpTimerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onPumpTimer() {
  portENTER_CRITICAL_ISR(&pumpTimerMux);
    digitalWrite(PUMP_DIMMER_M1_PIN, HIGH);
    delayMicroseconds(40);
    digitalWrite(PUMP_DIMMER_M1_PIN, LOW);
  
    timerWrite(pumpTimer, 50000);//10мс
  portEXIT_CRITICAL_ISR(&pumpTimerMux);
}

void handleInterrupt() // обработчик прерывания по фронту (от пина детекции фазы димера)
{
    power = 49000 - 478.5 * pumpPower;
    timerWrite(pumpTimer, power);
}

void setupPump()
{
  pinMode(PUMP_DIMMER_M2_PIN, INPUT_PULLUP);
  pinMode(PUMP_DIMMER_M1_PIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(PUMP_DIMMER_M2_PIN), handleInterrupt, RISING);

  pumpTimer = timerBegin(3, 80, true);
  timerAttachInterrupt(pumpTimer, &onPumpTimer, true);
}

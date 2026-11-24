// Standalone low-voltage pump phase timing diagnostic for Arduino-ESP32 3.3.12.
// Open THIS .ino as a separate sketch (do not compile with Gagguino).
// Connect GPIO23 (generator) to GPIO19 (simulated zero-cross input).
// ONLY USB and a short insulated 3.3 V GPIO-to-GPIO wire; NO mains or dimmer.
// GPIO18 is an output and MUST remain unconnected.
//
// Test plan: five automatic power levels, each with 6 s at 100 Hz followed
// by 4 s of silence; repeating. Serial Monitor: 115200 baud.
//
// This intentionally exercises the same timerAlarm() from a GPIO ISR path
// as the draft production pump. A crash/hang indicates a failed test.
#include <Arduino.h>

constexpr uint8_t ZC_IN = 19;
constexpr uint8_t ZC_GENERATOR = 23;
constexpr uint8_t GATE_OUT = 18;
constexpr uint32_t TIMER_HZ = 5000000;
constexpr uint8_t POWERS[] = {0, 25, 50, 75, 100};
constexpr uint8_t POWER_COUNT = sizeof(POWERS) / sizeof(POWERS[0]);
constexpr uint32_t GATE_PULSE_US = 40;
constexpr uint32_t HALF_CYCLE_US = 10000;
constexpr uint32_t TEST_ACTIVE_MS = 6000;
constexpr uint32_t TEST_PERIOD_MS = 10000;

hw_timer_t *phaseTimer = nullptr;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool pulsePending = false;
volatile uint32_t zeroCrossCount = 0;
volatile uint32_t gateCount = 0;
volatile uint32_t unexpectedTimerCount = 0;
volatile uint32_t overwrittenPendingCount = 0;
volatile uint32_t lastZeroCrossUs = 0;
volatile uint32_t minDelayUs = UINT32_MAX;
volatile uint32_t maxDelayUs = 0;
volatile uint32_t timingSamples = 0;
volatile uint8_t requestedPower = 0;

void ARDUINO_ISR_ATTR onGateTimer() {
  bool fire = false;
  portENTER_CRITICAL_ISR(&stateMux);
  if (pulsePending) {
    pulsePending = false;
    ++gateCount;
    // Software estimate, not an oscilloscope measurement.
    const uint32_t delayUs = (uint32_t)(micros() - lastZeroCrossUs);
    if (delayUs < minDelayUs) minDelayUs = delayUs;
    if (delayUs > maxDelayUs) maxDelayUs = delayUs;
    ++timingSamples;
    fire = true;
  } else {
    ++unexpectedTimerCount;
  }
  portEXIT_CRITICAL_ISR(&stateMux);
  if (fire) {
    digitalWrite(GATE_OUT, HIGH);
    delayMicroseconds(GATE_PULSE_US);
    digitalWrite(GATE_OUT, LOW);
  }
}

void ARDUINO_ISR_ATTR onZeroCross() {
  portENTER_CRITICAL_ISR(&stateMux);
  ++zeroCrossCount;
  lastZeroCrossUs = micros();
  if (pulsePending) ++overwrittenPendingCount;
  pulsePending = true;
  timerWrite(phaseTimer, 0);
  const uint32_t delayTicks = (uint32_t)(49000 - 478.5 * requestedPower);
  timerAlarm(phaseTimer, delayTicks, false, 0);
  portEXIT_CRITICAL_ISR(&stateMux);
}

void setup() {
  Serial.begin(115200);
  pinMode(GATE_OUT, OUTPUT);
  digitalWrite(GATE_OUT, LOW);
  pinMode(ZC_GENERATOR, OUTPUT);
  digitalWrite(ZC_GENERATOR, LOW);
  pinMode(ZC_IN, INPUT_PULLDOWN);

  phaseTimer = timerBegin(TIMER_HZ);
  if (!phaseTimer) {
    Serial.println("ERROR: timerBegin() failed");
    while (true) delay(1000);
  }
  timerAttachInterrupt(phaseTimer, &onGateTimer);
  // No timer alarm at boot: no gate pulse before the first zero-cross.
  attachInterrupt(digitalPinToInterrupt(ZC_IN), onZeroCross, RISING);

  Serial.printf("START: core=%s timerHz=%lu GPIO23 -> GPIO19 required\n",
                ESP.getSdkVersion(), (unsigned long)timerGetFrequency(phaseTimer));
  Serial.println("Cycle: 0,25,50,75,100%; 6 s active / 4 s silence each");
  Serial.println("GPIO18 output must remain unconnected");
}

void loop() {
  static uint32_t bootMs = millis();
  static uint32_t lastReportMs = millis();
  static uint32_t lastEdgeUs = micros();
  static bool generatorLevel = false;
  static bool previousActive = false;
  static uint8_t previousStage = 255;
  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = (uint32_t)(nowMs - bootMs);
  const uint8_t stage = (elapsedMs / TEST_PERIOD_MS) % POWER_COUNT;
  const uint32_t stageElapsedMs = elapsedMs % TEST_PERIOD_MS;
  const bool active = stageElapsedMs < TEST_ACTIVE_MS;

  if (stage != previousStage) {
    generatorLevel = false;
    digitalWrite(ZC_GENERATOR, LOW);
    previousActive = false;
    portENTER_CRITICAL(&stateMux);
    requestedPower = POWERS[stage];
    portEXIT_CRITICAL(&stateMux);
    previousStage = stage;
    Serial.printf("STAGE power=%u%% expectedDelay=%.2f us\n",
                  POWERS[stage], (49000 - 478.5 * POWERS[stage]) / 5.0);
  }

  if (!active) {
    if (generatorLevel) {
      generatorLevel = false;
      digitalWrite(ZC_GENERATOR, LOW);
    }
  } else if (!previousActive) {
    generatorLevel = true;
    digitalWrite(ZC_GENERATOR, HIGH);
    lastEdgeUs = micros();
  } else {
    const uint32_t nowUs = micros();
    const uint32_t interval = generatorLevel ? 2000 : (HALF_CYCLE_US - 2000);
    if ((uint32_t)(nowUs - lastEdgeUs) >= interval) {
      generatorLevel = !generatorLevel;
      digitalWrite(ZC_GENERATOR, generatorLevel ? HIGH : LOW);
      lastEdgeUs = nowUs;
    }
  }
  previousActive = active;

  if ((uint32_t)(nowMs - lastReportMs) >= 1000) {
    lastReportMs = nowMs;
    uint32_t crossings, fired, unexpected, overwritten, lo, hi, samples;
    bool pending;
    portENTER_CRITICAL(&stateMux);
    crossings = zeroCrossCount;
    fired = gateCount;
    unexpected = unexpectedTimerCount;
    overwritten = overwrittenPendingCount;
    pending = pulsePending;
    lo = minDelayUs;
    hi = maxDelayUs;
    samples = timingSamples;
    portEXIT_CRITICAL(&stateMux);
    Serial.printf("power=%u %s zc=%lu gate=%lu pending=%d unexpected=%lu overwritten=%lu delayUs(min/max)=%lu/%lu samples=%lu\n",
                  POWERS[stage], active ? "ACTIVE" : "SILENT",
                  (unsigned long)crossings, (unsigned long)fired, pending,
                  (unsigned long)unexpected, (unsigned long)overwritten,
                  (unsigned long)(samples ? lo : 0),
                  (unsigned long)hi, (unsigned long)samples);
  }
}

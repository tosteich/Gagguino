
#include <atomic>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "GaggiaPid.h"
#ifdef GAGGUINO_EXPERIMENTAL_BOILER_PID
#include "BoilerPid.h"
#endif
#include "webpage.h"
#include "Thermo_LMT01.h"
#include "TemperatureGuard.h"
#include "BenchTestConfig.h"
#include "Manometer.h"
#include "Boiler.h"
#include "timer.h"
#include "EasyNextionLibrary.h"
#include "StopWatch.h"
#include "PressureProfile.h"
#include "ManualPressure.h"
#include "LoopTimingStats.h"
#include "ShotArchive.h"
#include "UsbWebAssetUpdater.h"

#define BUTTON_PIN 32  //button
#define STEAM_BUTTON_PIN 26  //button
#define LMT_PIN 25
#define PUMP_DIMMER_M1_PIN 18  
#define PUMP_DIMMER_M2_PIN 19  
#define SENSORS_PERIOD 200
#define PUMP_PID_PERIOD 20
#define MAX_BOILER_PERIOD 1000

#define TARGET_TEMP 0
#define STEAM_TEMP 1
#define PID_KP 2
#define PID_KI 3
#define PID_KD 4
#define PUMP_KP 5
#define PUMP_KI 6
#define PUMP_KD 7
#define PREINFUSE_DURATION 8 
#define PREINFUSE_PRESSURE 9 
#define HOLD_DURATION 10 
#define HOLD_PRESSURE 11 
#define DECLINE_DURATION 12 
#define DECLINE_PRESSURE 13 

byte graphTemp=0;
byte graphPress=0;

const char* ssid = "KKK";            //Name of your home Wi-Fi
const char* password = "muzzleoid";  //Password of your home Wi-Fi
// AsyncTCP callbacks run in another FreeRTOS task.
std::atomic<unsigned int> wsClients{0};
float currentTemperature = 1;
float lastSentTemperature = 1;
float currentPressure = 0;
float lastSentPressure = 0;
int heatPower;
void setPumpPulsesEnabled(bool enabled); // Implemented in pump.ino.
volatile int pumpPower = 100;    
long resetTime = 0;
long currTimeInSec = 0;
// Default matches the Nextion mode initialized in setup().
boolean isBrewEnabled = true;
boolean isSteaming = false;
boolean isManual = false;
int manualPressure=0;
volatile bool timeToCheckCounter = false;
bool flowButtonIsPressed = false;
boolean lcdPauseButtonPressed = false;
int declineMatrix[60];

typedef struct { 
  uint8_t index;
  char* key;
  int value;
} setting;

setting settings[] {
    {TARGET_TEMP, "targetTemp", 0},
    {STEAM_TEMP, "steamTemp", 0},
    {PID_KP, "pidKp", 0},
    {PID_KI, "pidKi", 0},
    {PID_KD, "pidKd", 0},
    {PUMP_KP, "pumpKp", 0},
    {PUMP_KI, "pumpKi", 0},
    {PUMP_KD, "pumpKd", 0},
    {PREINFUSE_DURATION, "preinfDur", 0},
    {PREINFUSE_PRESSURE, "preinfPress", 0},
    {HOLD_DURATION, "holdDur", 0},
    {HOLD_PRESSURE, "holdPress", 0},    
    {DECLINE_DURATION, "declineDur", 0},
    {DECLINE_PRESSURE, "declinePress", 0}
};

Preferences preferences;
EasyNex myNex(Serial2);

#ifdef GAGGUINO_EXPERIMENTAL_BOILER_PID
BoilerPid boilerPid(SENSORS_PERIOD);
#else
GaggiaPid boilerPid(SENSORS_PERIOD);
#endif
GaggiaPid pumpPid(PUMP_PID_PERIOD);
Boiler boiler(MAX_BOILER_PERIOD);
LMT01 thermoLmt;
TemperatureGuard temperatureGuard;
Manometer manometer;

Timer pumpPidTimer(PUMP_PID_PERIOD);
Timer sensorsTimer(SENSORS_PERIOD);
Timer steamButtonTimer(100);
Timer displayTimer(1000);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


StopWatch stopWatch;
ShotArchive shotArchive;
#ifdef GAGGUINO_USB_WEB_UPDATE
UsbWebAssetUpdater usbWebAssetUpdater;
#endif
#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
// HTTP handlers only enqueue a request. The main loop builds test samples.
std::atomic<uint32_t> pendingShotTestMs{0};
// Batch request encoding: count in upper 16 bits, duration in seconds in lower 16.
std::atomic<uint32_t> pendingShotBatch{0};
// 0=idle, 1=queued, 2=running, 3=completed, 4=failed.
std::atomic<uint8_t> shotBatchState{0};
std::atomic<uint32_t> shotBatchTotal{0};
std::atomic<uint32_t> shotBatchQueued{0};
std::atomic<uint32_t> shotBatchSaved{0};
std::atomic<uint32_t> shotBatchFailed{0};
uint32_t shotBatchDurationMs=0;
uint32_t shotBatchBaseSaved=0;
uint32_t shotBatchBaseFailed=0;
#endif

// Web callbacks read published settings without accessing the live PID.
std::atomic<int> publishedTargetTemp{0};
std::atomic<int> publishedBoilerKp{0};
std::atomic<int> publishedBoilerKi{0};
std::atomic<int> publishedBoilerKd{0};

bool sensorsReadyThisLoop = false;
// Prevent stale PID state being reused when qualified LMT01 data returns.
bool boilerPidSensorWasFresh = false;
bool pumpPressureWasValid = false;
// Loop-owned gate state: avoid entering a FreeRTOS critical section on every
// 30-40 us loop iteration if the requested inhibit state has not changed.
bool pumpGateEnabledInLoop = true;
void updatePumpGateEnabled(bool enabled) {
  if (pumpGateEnabledInLoop == enabled) return;
  setPumpPulsesEnabled(enabled);
  pumpGateEnabledInLoop = enabled;
}
// AsyncTCP consumers use published snapshots, not loop-owned mutable state.
// Keep -50C as the original unqualified LMT01 web display value.
std::atomic<float> publishedCurrentTemperature{-50.0f};
std::atomic<float> publishedCurrentPressure{0.0f};
std::atomic<bool> pendingWebReboot{false};

// Diagnostic only: a long loop gap can delay heater safety decisions.
// It cannot replace an independent hardware thermal cutoff.
uint32_t previousLoopStartedMs = 0;
bool hasPreviousLoopStart = false;
uint32_t lastLoopGapWarningMs = 0;
LoopTimingStats loopTimingStats;

void loop() {
  const uint32_t loopStartedUs = micros();
  const uint32_t loopStartedMs = millis();
  loopTimingStats.begin(loopStartedUs, loopStartedMs);
  if (hasPreviousLoopStart) {
    const uint32_t gapMs = (uint32_t)(loopStartedMs - previousLoopStartedMs);
    // Report a long scheduling/blocking delay, no more than once per 10s.
    if (gapMs >= 1000 && (uint32_t)(loopStartedMs - lastLoopGapWarningMs) >= 10000) {
      Serial.printf("WARNING: main loop gap %lu ms; heater supervision delayed\n",
                    (unsigned long)gapMs);
      lastLoopGapWarningMs = loopStartedMs;
    }
  }
  hasPreviousLoopStart = true;
  previousLoopStartedMs = loopStartedMs;
  if (pendingWebReboot.exchange(false)) {
    // WebSocket callbacks must not restart the MCU while other tasks
    // may be changing heater / pump outputs. Inhibit both before reboot.
    updatePumpGateEnabled(false);
    pumpPower = 0;
    boiler.setHeatPower(0);
    ESP.restart();
    return;
  }
#ifdef GAGGUINO_USB_WEB_UPDATE
  // USB bench-only maintenance: disabled in production builds.
  usbWebAssetUpdater.poll(stopWatch.getIsFlow(), shotArchive.filesystemMutex());
#endif
  // Apply queued HTTP updates on the same task as PID calculations.
  applyPendingWebSettings();
  myNex.NextionListen();

  ws.cleanupClients();
  // StopWatch is owned by loop(), never written from AsyncTCP callbacks.
  stopWatch.setWsClients((byte)min(wsClients.load(), 255u));

  checkMachineButtonStatus();

  checkThermoPulsesCounter();
#ifdef GAGGUINO_LMT_BENCH_TEST
  reportBenchLmtSimulation();
#endif

  checkSteamButtonStatus();   

  sensorsReadyThisLoop = sensorsTimer.ready();

  updateVariablesFromSensorsData(); 
#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  // Requests are accepted by AsyncWebServer, but generation stays in loop().
  const uint32_t requestedTestMs = pendingShotTestMs.exchange(0);
  if (requestedTestMs != 0) {
    if (shotBatchState.load() == 1 || shotBatchState.load() == 2 ||
        stopWatch.getIsFlow() || !shotArchive.generateTestShot(requestedTestMs)) {
      Serial.println("SHOT_TEST: busy or physical flow active; test skipped");
    }
  }
  const uint32_t requestedBatch = pendingShotBatch.exchange(0);
  if (requestedBatch != 0 && shotBatchState.load() == 1) {
    shotBatchTotal.store(requestedBatch >> 16);
    shotBatchQueued.store(0);
    shotBatchSaved.store(0);
    shotBatchFailed.store(0);
    shotBatchDurationMs = (requestedBatch & 0xffff) * 1000UL;
    shotBatchBaseSaved = shotArchive.testSaved();
    shotBatchBaseFailed = shotArchive.testFailed();
    shotBatchState.store(2);
    Serial.printf("SHOT_TEST_BATCH: starting %lu shots, %lu ms each\n",
                  (unsigned long)shotBatchTotal.load(),
                  (unsigned long)shotBatchDurationMs);
  }
  if (shotBatchState.load() == 2) {
    const uint32_t saved = shotArchive.testSaved() - shotBatchBaseSaved;
    const uint32_t failed = shotArchive.testFailed() - shotBatchBaseFailed;
    shotBatchSaved.store(saved);
    shotBatchFailed.store(failed);
    if (failed != 0 || stopWatch.getIsFlow()) {
      shotBatchState.store(4);
      Serial.println("SHOT_TEST_BATCH: stopped (write failed or physical flow)");
    } else if (saved == shotBatchTotal.load()) {
      shotBatchState.store(3);
      Serial.println("SHOT_TEST_BATCH: completed");
    } else if (saved == shotBatchQueued.load()) {
      if (shotArchive.generateTestShot(shotBatchDurationMs)) {
        shotBatchQueued.fetch_add(1);
      } else {
        shotBatchState.store(4);
        Serial.println("SHOT_TEST_BATCH: unable to queue next shot");
      }
    }
  }
#endif

  // Capture one bounded sample at each 200 ms sensor tick; no flash I/O here.
  if (sensorsReadyThisLoop) {
    shotArchive.sample(stopWatch.getIsFlow(), stopWatch.getTimerTime(),
                       currentTemperature, currentPressure, heatPower, (int)pumpPower);
  }

  updatePumpPower();
  reportPumpProfileState();

  // Loss of qualified LMT01 data must disable heat between sensor updates.
  const bool temperatureFresh = temperatureGuard.fresh(millis());
  if (!temperatureFresh) {
    if (boilerPidSensorWasFresh) {
      boilerPid.resetState();
    }
    boilerPidSensorWasFresh = false;
    heatPower = 0;
    boiler.setHeatPower(0);
  } else {
    boilerPidSensorWasFresh = true;
  }
  boiler.updateHeater();

  firstRefresh();

  updateUi();

  LoopTimingSnapshot loopReport;
  if (loopTimingStats.end(micros(), millis(), loopReport)) {
    Serial.printf("LOOP_STATS max=%luus avg=%luus gap_max=%luus samples=%lu\n",
                  (unsigned long)loopReport.maxDurationUs,
                  (unsigned long)loopReport.avgDurationUs,
                  (unsigned long)loopReport.maxStartGapUs,
                  (unsigned long)loopReport.samples);
  }
}

void updateVariablesFromSensorsData() {
  if (sensorsReadyThisLoop) {
    currentTemperature = thermoLmt.getTemperature();
    currentPressure = manometer.getPressure();
    publishedCurrentTemperature.store(currentTemperature);
    publishedCurrentPressure.store(currentPressure);
    heatPower = 0;
    if (temperatureGuard.fresh(millis()) && currentTemperature > 0 && currentTemperature < 145) {
#ifdef GAGGUINO_LMT_BENCH_TEST
      heatPower = 100;  // USB-only bench: GPIO14 MUST be unconnected.
#else
      heatPower = boilerPid.getResult(currentTemperature);
#endif
    }
    boiler.setHeatPower(heatPower);
    stopWatch.run(ws, myNex);
  }
}

void updatePumpPower()
{
  // Preserve legacy unregulated pump behavior outside a PID-controlled shot.
  if (!stopWatch.getIsFlow() || !isBrewEnabled) {
    updatePumpGateEnabled(true);
    if (pumpPressureWasValid) {
      pumpPid.resetState();
      pumpPressureWasValid = false;
    }
    if (stopWatch.getIsFlow() && pumpPower != 100) {
      pumpPower = 100;
    }
    return;
  }

  if (sensorsReadyThisLoop) {
    if (isManual) pumpPid.setSetPoint(manualPressure);
    else managePumpPidSetPoint(stopWatch.getTimerTime());
  }

  if (!pumpPidTimer.ready()) return;

  // Every control-cycle sample must be checked before PID integration.
  // A value of 0% alone does not disable the phase gate!
  const float pressure = manometer.getPressure();
  if (!manometer.isValid()) {
    updatePumpGateEnabled(false);
    pumpPower = 0;
    pumpPid.resetState();
    pumpPressureWasValid = false;
    return;
  }

  if (!pumpPressureWasValid) pumpPid.resetState();
  pumpPressureWasValid = true;
  pumpPower = pumpPid.getResult(pressure * 10.0f);
  updatePumpGateEnabled(true);
}

// Diagnostic only: sample the active pump mode without changing its control
// path. UART is 9600 baud, so only enqueue a short line when TX has room.
void reportPumpProfileState() {
  static bool previousFlow = false;
  static bool previousMode = true;
  static uint32_t lastReportMs = 0;
  const bool flow = stopWatch.getIsFlow();
  const uint32_t now = millis();
  const bool transition = (flow != previousFlow) ||
                          (flow && isBrewEnabled != previousMode);
  const bool periodic = flow && (uint32_t)(now - lastReportMs) >= 2000;
  previousFlow = flow;
  previousMode = isBrewEnabled;
  if (!transition && !periodic) return;
  // Never hold up time-sensitive pump/heater work for Serial output.
  if (Serial.availableForWrite() < 90) return;
  lastReportMs = now;
  if (!flow) {
    Serial.println("PUMP_DIAG flow=0");
    return;
  }
  Serial.printf("PUMP_DIAG t=%lus pid=%u manual=%u sp=%.1f p=%.1f out=%d valid=%u\n",
    (unsigned long)(stopWatch.getTimerTime() / 1000u),
    (unsigned int)isBrewEnabled,
    (unsigned int)isManual,
    pumpPid.getSetPoint() / 10.0f,
    currentPressure,
    (int)pumpPower,
    (unsigned int)manometer.isValid());
}

void updateUi() {
  if (displayTimer.ready()) {
    updateGraphsInfo();
    // During brewing: shot duration. After release: elapsed time since completion.
    currTimeInSec = stopWatch.getCurrentTimeSeconds(millis());
    updateInfoToClients();
    sendInfoToDisplay();
  }
}

void sendInfoToDisplay() {
  if (lastSentTemperature != currentTemperature) {
    myNex.writeNum("temp", currentTemperature * 10);     
    myNex.writeNum("graphTemp", graphTemp); 
    lastSentTemperature = currentTemperature;
  }
  if (lastSentPressure != currentPressure) {
    myNex.writeNum("pressure", currentPressure * 10);
    myNex.writeNum("graphPress", graphPress);
    lastSentPressure = currentPressure;
  }
  myNex.writeNum("currentTime", currTimeInSec);
}

void firstRefresh() { 
  if(myNex.currentPageId != myNex.lastCurrentPageId) { 
    if(myNex.currentPageId == 1){
      myNex.writeNum("currentTime", currTimeInSec);
    }
    myNex.lastCurrentPageId = myNex.currentPageId; 
  }
}

void managePumpPidSetPoint(uint32_t flowTime)
{
  const PressureProfile profile{
    settings[PREINFUSE_DURATION].value,
    settings[PREINFUSE_PRESSURE].value,
    settings[HOLD_DURATION].value,
    settings[HOLD_PRESSURE].value,
    settings[DECLINE_DURATION].value,
    settings[DECLINE_PRESSURE].value
  };
  pumpPid.setSetPoint(pressureProfileSetpoint(profile, flowTime));
}

void trigger0() {
  loadSettingsFromNex();
  updatePreferenses();
  updateBoilerPidSettings();
  updatePumpPidSettings();
  initDeclineMatrix();
  publishWebSettings();
}

void trigger1() {
   updateManualPressure();
}

void trigger12() {
   isBrewEnabled = !isBrewEnabled;
   Serial.printf("BREW_MODE pid=%u (Nextion trigger12)\n",
                 (unsigned int)isBrewEnabled);
   // Do not carry pressure PID integral/derivative state across
   // controlled/unregulated mode changes.
   pumpPid.resetState();
   pumpPressureWasValid = false;
}

void trigger13() {
   if (!isManual) {
     pumpPid.resetState();
     pumpPressureWasValid = false;
   }
   isManual = true;
   updateManualPressure();
}

void updateManualPressure() {
   manualPressure = manualPressureTenthsFromNextion(myNex.readNumber("manualPressure"));  
}

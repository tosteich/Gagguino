
#include <atomic>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ESP32_FTPClient.h>
#include <AsyncTCP.h>
#include "GaggiaPid.h"
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

char ftp_server[] = "10.0.0.1";
char ftp_user[] = "gaggia";
char ftp_pass[] = "123";
const char* ssid = "KKK";            //Name of your home Wi-Fi
const char* password = "muzzleoid";  //Password of your home Wi-Fi
// AsyncTCP callbacks run in another FreeRTOS task.
std::atomic<unsigned int> wsClients{0};
float currentTemperature = 1;
float lastSentTemperature = 1;
float currentPressure = 0;
float lastSentPressure = 0;
int heatPower;
extern volatile bool pumpPulsesEnabled; // Defined in pump.ino.
volatile int pumpPower = 100;    
long resetTime = 0;
long currTimeInSec = 0;
boolean isBrewEnabled = false;
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
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

GaggiaPid boilerPid(SENSORS_PERIOD);
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

ESP32_FTPClient ftp(ftp_server, ftp_user, ftp_pass, 5000, 2);

StopWatch stopWatch;

// Web callbacks read published settings without accessing the live PID.
std::atomic<int> publishedTargetTemp{0};
std::atomic<int> publishedBoilerKp{0};
std::atomic<int> publishedBoilerKi{0};
std::atomic<int> publishedBoilerKd{0};

bool sensorsReadyThisLoop = false;
// Prevent stale PID state being reused when qualified LMT01 data returns.
bool boilerPidSensorWasFresh = false;
bool pumpPressureWasValid = false;
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

void loop() {
  const uint32_t loopStartedMs = millis();
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
    pumpPulsesEnabled = false;
    pumpPower = 0;
    boiler.setHeatPower(0);
    ESP.restart();
    return;
  }
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

  updatePumpPower();

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
    pumpPulsesEnabled = true;
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
    pumpPulsesEnabled = false;
    pumpPower = 0;
    pumpPid.resetState();
    pumpPressureWasValid = false;
    return;
  }

  if (!pumpPressureWasValid) pumpPid.resetState();
  pumpPressureWasValid = true;
  pumpPower = pumpPid.getResult(pressure * 10.0f);
  pumpPulsesEnabled = true;
}

void updateUi() {
  if (displayTimer.ready()) {
    updateGraphsInfo();
    currTimeInSec = (millis() - stopWatch.getResetTime()) / 1000;
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
}

void trigger1() {
   updateManualPressure();
}

void trigger12() {
   Serial.print("PRESSED, isBrewEnabled-");
   Serial.println(isBrewEnabled);
   isBrewEnabled = !isBrewEnabled;
}

void trigger13() {
   isManual = true;
   updateManualPressure();
}

void updateManualPressure() {
   manualPressure = myNex.readNumber("manualPressure") * 10 / 2;  
}

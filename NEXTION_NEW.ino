
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
#include "Manometer.h"
#include "Boiler.h"
#include "timer.h"
#include "EasyNextionLibrary.h"
#include "StopWatch.h"

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
volatile byte wsClients = 0;
float currentTemperature = 1;
float lastSentTemperature = 1;
float currentPressure = 0;
float lastSentPressure = 0;
int heatPower;
volatile int pumpPower = 100;    
long resetTime = 0;
long currTimeInSec = 0;
boolean isBrewEnabled = false;
boolean isSteaming = false;
boolean isManual = false;
int manualPressure=0;
boolean timeToCheckCounter = false;
boolean flowButtonIsPressed = false;
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
Manometer manometer;

Timer pumpPidTimer(PUMP_PID_PERIOD);
Timer sensorsTimer(SENSORS_PERIOD);
Timer steamButtonTimer(100);
Timer displayTimer(1000);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

ESP32_FTPClient ftp(ftp_server, ftp_user, ftp_pass, 5000, 2);

StopWatch stopWatch;

bool sensorsReadyThisLoop = false;

void loop() {
  myNex.NextionListen();

  ws.cleanupClients();

  checkMachineButtonStatus();

  checkThermoPulsesCounter();

  checkSteamButtonStatus();   

  sensorsReadyThisLoop = sensorsTimer.ready();

  updateVariablesFromSensorsData(); 

  updatePumpPower();

  boiler.updateHeater();

  firstRefresh();

  updateUi();
}

void updateVariablesFromSensorsData() {
  if (sensorsReadyThisLoop) {
    currentTemperature = thermoLmt.getTemperature();
    currentPressure = manometer.getPressure();
    heatPower = 0;
    if (currentTemperature > 0 && currentTemperature < 145) {
      heatPower = boilerPid.getResult(currentTemperature);
    }
    boiler.setHeatPower(heatPower);
    stopWatch.run(ws, myNex);
  }
}

void updatePumpPower()
{
    if (stopWatch.getIsFlow()) 
    {  
      if (isBrewEnabled) 
      {
        if (sensorsReadyThisLoop)
        {
          if(isManual) 
          {
            pumpPid.setSetPoint(manualPressure);  
          } 
          else 
          {
            managePumpPidSetPoint(stopWatch.getTimerTime());            
          }
        }
        if (pumpPidTimer.ready())
        {
           pumpPower = pumpPid.getResult(manometer.getPressure() * 10);            
        }
      } 
      else if (pumpPower != 100)
      {
        pumpPower = 100;
      } 
    }
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

void managePumpPidSetPoint(int flowTime)
{
    float passedTime = flowTime / 1000.0;
    if (passedTime < settings[PREINFUSE_DURATION].value)
    {
       pumpPid.setSetPoint(settings[PREINFUSE_PRESSURE].value);
       return; 
    } 
    int holdTime = settings[PREINFUSE_DURATION].value + settings[HOLD_DURATION].value;
    if (passedTime < holdTime)
    {
       pumpPid.setSetPoint(settings[HOLD_PRESSURE].value);
       return; 
    }
    int declineTime = (int) passedTime - holdTime;
    const int matrixSize = sizeof(declineMatrix) / sizeof(declineMatrix[0]);
    if (declineTime >= 0 && declineTime < settings[DECLINE_DURATION].value && declineTime < matrixSize)
    {
        pumpPid.setSetPoint(declineMatrix[declineTime]);
    }
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

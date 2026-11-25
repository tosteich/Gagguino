#pragma once
#include <stdint.h>
class StopWatch {
private:
  uint32_t startTime = 0;
  bool started = false;
  uint32_t resetTime = 0;
  byte timerSecs = 0;
  byte timerMils = 0;
  byte wsClients = 0;
  byte memoryTimer = 40;
  int flowTickCounter = 0;
  boolean isFlow = false;
  boolean flowJustPressed = false;
  uint32_t timerTime = 0;

  void sendWsMessage(AsyncWebSocket &ws) {
    if (wsClients > 0) {
      char buffer[100];
      sprintf(buffer, "{\"name\":\"time\",\"state\":\"%s\",\"value\":\"%f\"}", isFlow ? "FLOW" : "Timer", timerTime / 1000.0);
      ws.textAll(buffer);
    }
  }

  void sendTimerTimeToNex(EasyNex &myNex) {
    myNex.writeNum("timerTime", timerTime / 100);
  }

public:
  void run(AsyncWebSocket &ws, EasyNex &myNex) {
    if (isFlow) {
      flowTickCounter++;
      const uint32_t currTime = millis();
      if (!started) {
        startTime = currTime;
        resetTime = currTime; // The clock begins at shot start, not at a later UI update.
        started = true;
        memoryTimer = 0;
      }
      timerTime = (uint32_t)(currTime - startTime);
      sendTimerTimeToNex(myNex);
      if (flowTickCounter % 2 == 0) {
        sendWsMessage(ws);
      } else if (flowJustPressed) {
        sendWsMessage(ws);
        if (flowTickCounter > 2) {
          setFlowJustPressed(false);
          myNex.writeNum("flowJustPressed", false);
        }
      }
    } else {
      if (memoryTimer == 0) {
        flowTickCounter = 0;
        startTime = 0;
        started = false;
      }
      if (memoryTimer < 40 && timerTime > 0) {
        if (flowJustPressed) {
          sendWsMessage(ws);
          sendTimerTimeToNex(myNex);
          setFlowJustPressed(false);
          myNex.writeNum("flowJustPressed", false);
        }
        memoryTimer++;
      } else if (memoryTimer == 40 && timerTime > 0) {
        timerTime = 0;
        sendWsMessage(ws);
        sendTimerTimeToNex(myNex);
        myNex.writeStr("page 1");
        memoryTimer++;
      }
    }
  }

  void setWsClients(byte wsClients) {
    this->wsClients = wsClients;
  }

  void setIsFlow(boolean isFlow) {
    this->isFlow = isFlow;
  }

  boolean getIsFlow() {
    return isFlow;
  }

  uint32_t getTimerTime() {
    return timerTime;
  }
  void setFlowJustPressed(boolean flowJustPressed) {
    this->flowJustPressed = flowJustPressed;
  }

  boolean getFlowJustPressed() {
    return flowJustPressed;
  }

  uint32_t getResetTime() {
    return resetTime;
  }
};

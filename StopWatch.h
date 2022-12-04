class StopWatch {
private:
  long startTime = 0;
  long resetTime = 0;
  byte timerSecs = 0;
  byte timerMils = 0;
  byte wsClients = 0;
  byte memoryTimer = 40;
  int flowTickCounter = 0;
  boolean isFlow = false;
  boolean flowJustPressed = false;
  int timerTime;

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
      long currTime = millis();
      if (startTime == 0) {
        startTime = currTime;
        memoryTimer = 0;
      }
      timerTime = currTime - startTime;
      sendTimerTimeToNex(myNex);
      if (flowTickCounter % 2 == 0) {
        sendWsMessage(ws);
      } else if (flowJustPressed) {
        sendWsMessage(ws);
        if (flowTickCounter > 2) {
          setFlowJustPressed(false);
          myNex.writeNum("flowJustPressed", false);
        }
        resetTime = currTime;
      }
    } else {
      if (memoryTimer == 0) {
        flowTickCounter = 0;
        startTime = 0;
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

  int getTimerTime() {
    return timerTime;
  }
  void setFlowJustPressed(boolean flowJustPressed) {
    this->flowJustPressed = flowJustPressed;
  }

  boolean getFlowJustPressed() {
    return flowJustPressed;
  }

  long getResetTime() {
    return resetTime;
  }
};

#include "FlowButtonState.h"

// Original button timing: immediate press, release after more than 50 ms.
// Handle the button in loop(), not from timer interrupts.
FlowButtonState flowButtonState(50);

void checkMachineButtonStatus() {
  flowButtonIsPressed = flowButtonState.update(digitalRead(BUTTON_PIN) == LOW, millis());
  if (flowButtonIsPressed) {
    if (!stopWatch.getIsFlow()) {
      setBooleans(true, true);
      pumpPid.setSetPoint(settings[PREINFUSE_PRESSURE].value); 
      stopWatch.run(ws, myNex);
    }
  } else {
    if (stopWatch.getIsFlow()) {
      setBooleans(false, true);
      isManual=false;
      stopWatch.run(ws, myNex);
    }
  }
}

void setBooleans(boolean isFlow, boolean flowJustPressed) {
  stopWatch.setIsFlow(isFlow);
  myNex.writeNum("isFlow", isFlow);
  stopWatch.setFlowJustPressed(flowJustPressed);
  myNex.writeNum("flowJustPressed", flowJustPressed);
}

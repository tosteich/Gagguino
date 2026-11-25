#include "FlowButtonState.h"

// Original button timing: immediate press, release after more than 50 ms.
// Handle the button in loop(), not from timer interrupts.
FlowButtonState flowButtonState(50);

void checkMachineButtonStatus() {
  flowButtonIsPressed = flowButtonState.update(digitalRead(BUTTON_PIN) == LOW, millis());
  if (flowButtonIsPressed) {
    if (!stopWatch.getIsFlow()) {
      // A new shot must not reuse accumulated PID state from a previous shot.
      pumpPid.resetState();
      setBooleans(true, true);
      pumpPid.setSetPoint(settings[PREINFUSE_PRESSURE].value);
      stopWatch.run(ws, myNex);
    }
  } else {
    if (stopWatch.getIsFlow()) {
      // Clear stale integral/derivative terms before the next shot.
      pumpPid.resetState();
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

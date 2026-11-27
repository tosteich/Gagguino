
boolean steamButtonJustPressed = false;
int prevTemp = 0;

void checkSteamButtonStatus() {
  if (steamButtonTimer.ready()) {
    if (!digitalRead(STEAM_BUTTON_PIN)) {
      if (!steamButtonJustPressed) {
        steamButtonJustPressed = true;
        isSteaming = true;
        boilerPid.resetState();
#ifndef GAGGUINO_EXPERIMENTAL_BOILER_PID
        if (shadowBoilerInitialized) shadowBoilerPid.resetState();
#endif
        boilerPid.setSetPoint(settings[STEAM_TEMP].value);
      }
    } else {
      if (steamButtonJustPressed) {
        steamButtonJustPressed = false;
        isSteaming = false;
        boilerPid.resetState();
#ifndef GAGGUINO_EXPERIMENTAL_BOILER_PID
        if (shadowBoilerInitialized) shadowBoilerPid.resetState();
#endif
        boilerPid.setSetPoint(settings[TARGET_TEMP].value);
      }
    }
  }
}

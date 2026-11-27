
boolean steamButtonJustPressed = false;
int prevTemp = 0;

void checkSteamButtonStatus() {
  if (steamButtonTimer.ready()) {
    if (!digitalRead(STEAM_BUTTON_PIN)) {
      if (!steamButtonJustPressed) {
        steamButtonJustPressed = true;
        isSteaming = true;
#ifdef GAGGUINO_EXPERIMENTAL_BOILER_PID
        experimentalBoilerOvertemp.cancelSteamCooldown();
#endif
        boilerPid.resetState();
        if (shadowBoilerInitialized) shadowBoilerPid.resetState();
        boilerPid.setSetPoint(settings[STEAM_TEMP].value);
      }
    } else {
      if (steamButtonJustPressed) {
        steamButtonJustPressed = false;
        isSteaming = false;
#ifdef GAGGUINO_EXPERIMENTAL_BOILER_PID
        experimentalBoilerOvertemp.beginSteamCooldown(
            boilerPid.getSetPoint(), settings[TARGET_TEMP].value);
#endif
        boilerPid.resetState();
        if (shadowBoilerInitialized) shadowBoilerPid.resetState();
        boilerPid.setSetPoint(settings[TARGET_TEMP].value);
      }
    }
  }
}

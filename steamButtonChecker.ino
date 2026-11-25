
boolean steamButtonJustPressed = false;
int prevTemp = 0;

void checkSteamButtonStatus() {
  if (steamButtonTimer.ready()) {
    if (!digitalRead(STEAM_BUTTON_PIN)) {
      if (!steamButtonJustPressed) {
        steamButtonJustPressed = true;
        isSteaming = true;
        boilerPid.resetState();
        boilerPid.setSetPoint(settings[STEAM_TEMP].value);
      }
    } else {
      if (steamButtonJustPressed) {
        steamButtonJustPressed = false;
        isSteaming = false;
        boilerPid.resetState();
        boilerPid.setSetPoint(settings[TARGET_TEMP].value);
      }
    }
  }
}

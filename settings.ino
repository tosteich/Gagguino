void updateSettingValueOnNex(int valueIndex)
{
   myNex.writeNum(settings[valueIndex].key, settings[valueIndex].value);
}

void loadSettingsFromNex()
{
  for(uint8_t i = 0; i < sizeof(settings)/sizeof(setting); ++i) {
    readSettingFromNex(i);
  }
}

 void readSettingFromNex(int index) 
{
   settings[index].value = myNex.readNumber(settings[index].key);  
}

void updatePreferenses()
{
  preferences.begin("gaggia-prefs", false);

  for(uint8_t i = 0; i < sizeof(settings)/sizeof(setting); ++i) {
    if (settings[i].value != preferences.getInt(settings[i].key, 0))
    {
        preferences.putInt(settings[i].key, settings[i].value);
    }
  }
  
  preferences.end();
}

void getPreferenses() {
  preferences.begin("gaggia-prefs", false);

  for(uint8_t i = 0; i < sizeof(settings)/sizeof(setting); ++i) {
      settings[i].value = preferences.getInt(settings[i].key, 0);
  }
  
  preferences.end();
}

void updateBoilerPidSettings() {
  // New tuning/target must not inherit integral or derivative history.
  boilerPid.resetState();
  boilerPid.setupPid(settings[PID_KP].value / 10.f,  
                     settings[PID_KI].value / 10.f,  
                     settings[PID_KD].value / 10.f,
                     isSteaming ? settings[STEAM_TEMP].value : settings[TARGET_TEMP].value);   
}

void updatePumpPidSettings() {
  pumpPid.setupPid(settings[PUMP_KP].value / 10.f,  
                   settings[PUMP_KI].value / 10.f,  
                   settings[PUMP_KD].value / 10.f,
                   settings[HOLD_PRESSURE].value);   
}

void initDeclineMatrix()
{
  const int matrixSize = sizeof(declineMatrix) / sizeof(declineMatrix[0]);
  const int duration = constrain(settings[DECLINE_DURATION].value, 0, matrixSize);
  if (duration <= 0) {
    return;
  }

  const int startPressure = settings[HOLD_PRESSURE].value;
  const int endPressure = settings[DECLINE_PRESSURE].value;
  const int64_t pressureDelta = (int64_t)endPressure - startPressure;

  for (int i = 0; i < duration; ++i) {
    // Use the total change rather than truncating a per-second step.
    // The final element reaches endPressure exactly.
    declineMatrix[i] = startPressure + (int)(pressureDelta * (i + 1) / duration);
  }
}

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
  if (settings[DECLINE_DURATION].value > 0)
  {
      int step = (settings[HOLD_PRESSURE].value - settings[DECLINE_PRESSURE].value) / settings[DECLINE_DURATION].value; 

      declineMatrix[0] = settings[HOLD_PRESSURE].value - step;
      for(uint8_t i = 1; i < settings[DECLINE_DURATION].value; ++i) {
          declineMatrix[i] = declineMatrix[i - 1] - step;
      }
  }
}    

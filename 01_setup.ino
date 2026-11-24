void setup(void) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STEAM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LMT_PIN, INPUT);

  Serial.begin(9600);
  Serial2.begin(9600);

  getPreferenses();
  updateBoilerPidSettings();   
  updatePumpPidSettings();   
  setupPump();                

  Timer wifiTimer(500);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");

  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(7200);
  updateCurrDate();

  initWebSocket();
  initWebServerControllers();
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();

  myNex.writeStr("page 0");
  delay(50);
  myNex.writeNum("isBrewEnabled", false);

  myNex.lastCurrentPageId = 1;
  

  for(uint8_t i = 0; i < sizeof(settings)/sizeof(setting); ++i) {
      updateSettingValueOnNex(i);
  }

  initPulseCounter();
  initPulseCounterTimer();
#ifdef GAGGUINO_LMT_BENCH_TEST
  setupBenchLmtSimulation();
#endif
  initDeclineMatrix();

  for(uint8_t i = 0; i < sizeof(settings)/sizeof(setting); ++i) {
       Serial.print(settings[i].key);
       Serial.print(" - ");
       Serial.println(settings[i].value);
  }
}
      

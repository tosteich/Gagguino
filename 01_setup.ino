void setup(void) {
  // Establish heater GPIO14 LOW before initializing any other subsystem.
  boiler.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STEAM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LMT_PIN, INPUT);

  Serial.begin(9600);
  // Nextion wiring: ESP32 UART2 RX=GPIO16, TX=GPIO17.
  // Explicit pins are mandatory: the core's defaults may assign TX to
  // GPIO25, which is reserved for the LMT01 PCNT input.
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

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

  // Start asynchronous SNTP sync; never wait for networking in setup().
  // Shot metadata stores UTC, while the browser handles Europe/Kyiv DST.
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  initWebFiles();
  shotArchive.begin();
  thermalTraceMutex = xSemaphoreCreateMutex();
  if (!thermalTraceMutex) Serial.println("THERMAL_TRACE: mutex creation failed");
  initBoilerWebCommandQueue();
  initFullWebSettingsQueue();
  initWebSocket();
  initWebServerControllers();
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();

  myNex.writeStr("page 0");
  delay(50);
  // Mirror the ESP32 control mode on Nextion at startup.
  myNex.writeNum("isBrewEnabled", isBrewEnabled);

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
      

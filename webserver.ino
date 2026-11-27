#include <memory>
#include <new>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// AsyncWebServer callbacks execute outside the Arduino loop task.
// Only enqueue validated changes here; the loop owns PID, settings,
// Preferences and Nextion writes.
struct BoilerWebCommand {
  uint8_t index;
  int value;
};
static QueueHandle_t boilerWebCommandQueue = nullptr;

void initBoilerWebCommandQueue() {
  boilerWebCommandQueue = xQueueCreate(8, sizeof(BoilerWebCommand));
  if (!boilerWebCommandQueue) {
    Serial.println("ERROR: unable to create boiler web command queue");
  }
}

bool enqueueBoilerWebCommand(uint8_t index, int value) {
  if (!boilerWebCommandQueue) return false;
  const BoilerWebCommand command{index, value};
  return xQueueSend(boilerWebCommandQueue, &command, 0) == pdTRUE;
}

// Web settings snapshot (14 entries) and one complete configuration command.
// HTTP callbacks never read live 'settings' or touch Preferences/PID/Nextion.
constexpr uint8_t WEB_SETTING_COUNT = 14;
struct WebSettingsCommand { int values[WEB_SETTING_COUNT]; };
static QueueHandle_t fullWebSettingsQueue = nullptr;
static std::atomic<int> publishedWebSettings[WEB_SETTING_COUNT];
static std::atomic<bool> publishedFlowActive{false};
static std::atomic<uint32_t> webSettingsRevision{0};

void publishWebSettings() {
  for (uint8_t i = 0; i < WEB_SETTING_COUNT; ++i)
    publishedWebSettings[i].store(settings[i].value);
  webSettingsRevision.fetch_add(1);
}

void initFullWebSettingsQueue() {
  fullWebSettingsQueue = xQueueCreate(1, sizeof(WebSettingsCommand));
  publishWebSettings();
}

static bool parseWebSetting(const String &raw, uint8_t index, int &value) {
  if (raw.isEmpty() || raw.length() > 12) return false;
  char* end = nullptr;
  const double n = strtod(raw.c_str(), &end);
  if (end == raw.c_str() || *end != '\0' || !isfinite(n)) return false;
  const bool decimal = index == PID_KP || index == PID_KI || index == PID_KD ||
      index == PUMP_KP || index == PUMP_KI || index == PUMP_KD ||
      index == PREINFUSE_PRESSURE || index == HOLD_PRESSURE || index == DECLINE_PRESSURE;
  const double scaled = n * (decimal ? 10.0 : 1.0);
  if (scaled < 0 || scaled > 100000 || fabs(scaled - round(scaled)) > 0.00001) return false;
  const int v = (int)round(scaled);
  if (sanitizeBoilerSetting(index, v) != v) return false;
  // Explicit minimum temperatures to avoid accidental disabling or overheating.
  if (index == TARGET_TEMP && (v < 20 || v > 105)) return false;
  if (index == STEAM_TEMP && (v < 100 || v > 144)) return false;
  value = v;
  return true;
}

void applyPendingFullWebSettings() {
  if (!fullWebSettingsQueue || stopWatch.getIsFlow()) return;
  WebSettingsCommand command;
  if (xQueueReceive(fullWebSettingsQueue, &command, 0) != pdTRUE) return;
  // Serializes configuration changes with the main loop and Nextion.
  bool changedBoiler = false, changedPump = false, changedProfile = false;
  preferences.begin("gaggia-prefs", false);
  for (uint8_t i = 0; i < WEB_SETTING_COUNT; ++i) {
    if (settings[i].value == command.values[i]) continue;
    settings[i].value = command.values[i];
    preferences.putInt(settings[i].key, settings[i].value);
    updateWebSettingOnNex(i);
    if (i == TARGET_TEMP || i == STEAM_TEMP || i == PID_KP || i == PID_KI || i == PID_KD)
      changedBoiler = true;
    if (i == PUMP_KP || i == PUMP_KI || i == PUMP_KD) changedPump = true;
    if (i >= PREINFUSE_DURATION) changedProfile = true;
  }
  preferences.end();
  if (changedBoiler) updateBoilerPidSettings();
  if (changedPump) updatePumpPidSettings();
  if (changedProfile) initDeclineMatrix();
  publishWebSettings();
}

void applyPendingWebSettings() {
  publishedFlowActive.store(stopWatch.getIsFlow());
  applyPendingFullWebSettings();
  if (!boilerWebCommandQueue || stopWatch.getIsFlow()) return;
  BoilerWebCommand command;
  for (int i = 0; i < 8 && xQueueReceive(boilerWebCommandQueue, &command, 0) == pdTRUE; ++i) {
    switch (command.index) {
      case TARGET_TEMP: changeTargetTemp((byte)command.value); break;
      case PID_KP: changePidKp(command.value); break;
      case PID_KI: changePidKi(command.value); break;
      case PID_KD: changePidKd(command.value); break;
      default: break;
    }
    publishWebSettings();
  }
}

// Returns a nonnegative fixed-point PID gain (tenths), or false.
// Reject garbage/trailing characters, NaN, infinity and out-of-range values.
bool parseBoilerPidGain(const String& input, int& storedGain) {
  if (input.isEmpty()) return false;
  const char* begin = input.c_str();
  char* end = nullptr;
  const double gain = strtod(begin, &end);
  if (end == begin || *end != '\0' || !isfinite(gain) ||
      gain < 0.0 || gain > (double)MAX_STORED_PID_GAIN / 10.0) return false;
  storedGain = (int)(gain * 10.0 + 0.5);
  return true;
}

String processor(const String &var) {
  if (var == "TEMPERATUREC") {
    return getTemperature();
  }
  if (var == "PRESSURE") {
    return getPressure();
  }
  if (var == "TARGETTEMP") {
    return String(publishedTargetTemp.load());
  }
  if (var == "PID_KP") {
    return String(publishedBoilerKp.load() / 10.0f);
  }
  if (var == "PID_KI") {
    return String(publishedBoilerKi.load() / 10.0f);
  }
  if (var == "PID_KD") {
    return String(publishedBoilerKd.load() / 10.0f);
  }
  return String();
}

String getTemperature() {
  return String(publishedCurrentTemperature.load());
}

String getPressure() {
  const float pressure = publishedCurrentPressure.load();
  return String(pressure < 0 ? 0.0f : pressure);
}

void changeTargetTemp(byte temp) {
  settings[TARGET_TEMP].value = temp;
  preferences.begin("gaggia-prefs", false);
  preferences.putInt(settings[TARGET_TEMP].key, temp);
  preferences.end();
  sendWsTargetTemp();
  updateBoilerPidSettings();
  updateSettingValueOnNex(TARGET_TEMP);
}

void sendWsTargetTemp() {
  if (wsClients.load() > 0) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "{\"name\":\"targetTemp\",\"value\":\"%u\"}", settings[TARGET_TEMP].value);
    ws.textAll(buffer);
  }
}

void changePidKp(int newKp) {
  settings[PID_KP].value = newKp;
  preferences.begin("gaggia-prefs", false);
  preferences.putInt(settings[PID_KP].key, newKp);
  preferences.end();
  updateBoilerPidSettings();
  if (wsClients > 0) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "{\"name\":\"pidKp\",\"value\":\"%f\"}", newKp / 10.f);
    ws.textAll(buffer);
  }
  updateSettingValueOnNex(PID_KP);
}

void changePidKi(int newKi) {
  settings[PID_KI].value = newKi;
  preferences.begin("gaggia-prefs", false);
  preferences.putInt(settings[PID_KI].key, newKi);
  preferences.end();
  updateBoilerPidSettings();
  if (wsClients > 0) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "{\"name\":\"pidKi\",\"value\":\"%f\"}", newKi / 10.f);
    ws.textAll(buffer);
  }
  updateSettingValueOnNex(PID_KI);
}

void changePidKd(int newKd) {
  settings[PID_KD].value = newKd;
  preferences.begin("gaggia-prefs", false);
  preferences.putInt(settings[PID_KD].key, newKd);
  preferences.end();
  updateBoilerPidSettings();
  if (wsClients > 0) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "{\"name\":\"pidKd\",\"value\":\"%f\"}", newKd / 10.f);
    ws.textAll(buffer);
  }
  updateSettingValueOnNex(PID_KD);
}

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    // WebSocket payload is length-delimited; do not write a null byte past it.
    if (len == sizeof("reboot") - 1 && memcmp(data, "reboot", len) == 0) {
      pendingWebReboot.store(true);
    }
    if (len == sizeof("initInfo") - 1 && memcmp(data, "initInfo", len) == 0) {
      giveInitInfoToClient(client);
    }
  }
}

void giveInitInfoToClient(AsyncWebSocketClient *client) {
  char buffer[200];
  snprintf(buffer, sizeof(buffer), "{\"name\":\"initInfo\",\"currentTemp\":\"%f\",\"currentPress\":\"%f\",\"targetTemp\":\"%u\",\"pidKp\":\"%f\",\"pidKi\":\"%f\",\"pidKd\":\"%f\"}",
          publishedCurrentTemperature.load(), publishedCurrentPressure.load(), publishedTargetTemp.load(), publishedBoilerKp.load() / 10.0f, publishedBoilerKi.load() / 10.0f, publishedBoilerKd.load() / 10.0f);
  ws.text(client->id(), buffer);
}

void updateInfoToClients() {
  if (wsClients > 0) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"name\":\"updateInfo\",\"currentTemp\":\"%f\",\"currentPress\":\"%f\",\"currentPower\":\"%u\",\"currentTime\":\"%ld\"}",
            currentTemperature, currentPressure, heatPower, currTimeInSec);
    ws.textAll(buffer);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      const unsigned int connected = wsClients.fetch_add(1) + 1;
      Serial.printf("Total connected clients - %u\n", connected);
      break;
    }
    case WS_EVT_DISCONNECT: {
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      // Saturate at zero rather than underflow on an unexpected event.
      unsigned int previous = wsClients.load();
      while (previous > 0 && !wsClients.compare_exchange_weak(previous, previous - 1)) {}
      Serial.printf("Total connected clients - %u\n", previous > 0 ? previous - 1 : 0);
      break;
    }
    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Filesystem is optional: keep the original PROGMEM page as a fallback.
static bool darkWebFilesReady = false;

void initWebFiles() {
  if (!LittleFS.begin(false)) { // Never auto-format: preserve user files.
    Serial.println("LittleFS not mounted; serving legacy embedded page");
    return;
  }
#ifdef GAGGUINO_USB_WEB_UPDATE
  UsbWebAssetUpdater::recover();
#endif
  darkWebFilesReady = LittleFS.exists("/index.html") &&
                      LittleFS.exists("/css/style.css") &&
                      LittleFS.exists("/js/app.js");
  if (!darkWebFilesReady) {
    Serial.println("Dark UI assets missing; serving legacy embedded page");
  } else {
    Serial.println("Dark UI available from LittleFS");
  }
}

void initWebServerControllers() {
  if (darkWebFilesReady) {
    server.serveStatic("/css/", LittleFS, "/css/");
    server.serveStatic("/js/", LittleFS, "/js/");
  }
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (darkWebFilesReady) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send_P(200, "text/html", index_html, processor);
    }
  });

  // Diagnostic-only thermal trace. Capture stays in RAM; web callbacks
  // copy a completed trace briefly under mutex, then stream CSV without locks.
  server.on("/api/thermal-trace/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!thermalTraceMutex || xSemaphoreTake(thermalTraceMutex, 0) != pdTRUE) {
      request->send(503, "application/json", "{\"error\":\"busy\"}");
      return;
    }
    const ThermalTraceCore::State state=thermalTrace.state();
    const uint16_t count=thermalTrace.count();
    xSemaphoreGive(thermalTraceMutex);
    const char *name=state==ThermalTraceCore::READY?"ready":
                     state==ThermalTraceCore::POST?"post":
                     state==ThermalTraceCore::FLOW?"flow":"idle";
    const String body=String("{\"state\":\"")+name+"\",\"samples\":"+String(count)+"}";
    AsyncWebServerResponse *response=request->beginResponse(200,"application/json",body);
    response->addHeader("Cache-Control","no-store");
    request->send(response);
  });
  server.on("/api/thermal-trace.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!thermalTraceMutex || xSemaphoreTake(thermalTraceMutex,0)!=pdTRUE) {
      request->send(503,"text/plain","Trace busy");
      return;
    }
    const bool ready=thermalTrace.state()==ThermalTraceCore::READY;
    if (!ready) {
      xSemaphoreGive(thermalTraceMutex);
      request->send(409,"text/plain","Complete one shot and wait 60 seconds");
      return;
    }
    ThermalTraceCore *copy=new(std::nothrow) ThermalTraceCore(thermalTrace);
    xSemaphoreGive(thermalTraceMutex);
    if (!copy) {
      request->send(503,"text/plain","Not enough RAM for trace snapshot");
      return;
    }
    std::shared_ptr<ThermalTraceCore> snapshot(copy);
    AsyncWebServerResponse *response=request->beginChunkedResponse(
      "text/csv",
      [snapshot](uint8_t *buffer,size_t maxLen,size_t index)->size_t {
        const char *header="time_s,temperature_c,pressure_bar,heater_percent,pump_percent,phase\\n";
        size_t cursor=0, written=0;
        auto output=[&](const char *part,size_t length) {
          if (cursor+length>index && written<maxLen) {
            const size_t offset=index>cursor?index-cursor:0;
            const size_t remaining=length-offset;
            const size_t amount=remaining<maxLen-written?remaining:maxLen-written;
            memcpy(buffer+written,part+offset,amount);
            written+=amount;
          }
          cursor+=length;
        };
        output(header,strlen(header));
        for(uint16_t i=0;i<snapshot->count() && written<maxLen;++i) {
          const ThermalTraceCore::Sample& p=snapshot->at(i);
          char line[100];
          const int length=snprintf(line,sizeof(line),
             "%.3f,%.1f,%.2f,%u,%u,%s\\n",
             p.relativeMs/1000.0f,p.temperature10/10.0f,
             p.pressure100/100.0f,(unsigned int)p.heater,
             (unsigned int)p.pump,
             p.phase==ThermalTraceCore::BEFORE?"pre":
             p.phase==ThermalTraceCore::BREW?"brew":"post");
          if(length>0 && (size_t)length<sizeof(line))
             output(line,(size_t)length);
        }
        return written;
      });
    response->addHeader("Content-Disposition","attachment; filename=\"gagguino-thermal-trace.csv\"");
    response->addHeader("Cache-Control","no-store");
    request->send(response);
  });

  // Read-only filesystem diagnostics; 503 means archive writer is busy.
  server.on("/api/system/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    size_t total = 0, used = 0;
    uint16_t shots = 0;
    if (!shotArchive.storageStats(total, used, shots)) {
      request->send(503, "application/json",
                    "{\"error\":\"LittleFS unavailable or busy\"}");
      return;
    }
    const size_t freeBytes = used <= total ? total - used : 0;
    const String json = String("{\"totalBytes\":") + String((unsigned long)total) +
        ",\"usedBytes\":" + String((unsigned long)used) +
        ",\"freeBytes\":" + String((unsigned long)freeBytes) +
        ",\"shots\":" + String(shots) +
        ",\"maxShots\":" + String(ShotArchive::MAX_SHOTS) +
        ",\"writeErrors\":" + String((unsigned long)shotArchive.writeErrors()) +
        "}";
    AsyncWebServerResponse *response =
        request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  // Archive reads are bounded to 60 shots x 600 packed samples.
  // The writer owns flash writes; web handlers briefly take its mutex for reads.
  server.on("/api/shots", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json;
    if (!shotArchive.list(json)) {
      request->send(503, "text/plain", "Shot archive unavailable or busy");
      return;
    }
    request->send(200, "application/json", json);
  });
  server.on("/api/shot", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id")) {
      request->send(400, "text/plain", "Missing shot id");
      return;
    }
    const String value=request->getParam("id")->value();
    if(value.isEmpty() || value.length()>8) {
      request->send(400, "text/plain", "Invalid shot id");
      return;
    }
    for(size_t i=0;i<value.length();++i){
      if(value[i]<'0'||value[i]>'9'){
        request->send(400, "text/plain", "Invalid shot id");
        return;
      }
    }
    const uint32_t id=(uint32_t)value.toInt();
    uint8_t *bytes=nullptr;
    size_t length=0;
    if(!shotArchive.read(id,bytes,length)){
      request->send(404, "text/plain", "Shot missing or archive busy");
      return;
    }
    AsyncResponseStream *response=request->beginResponseStream("application/octet-stream");
    response->addHeader("Cache-Control","no-store");
    const size_t written=response->write(bytes,length);
    free(bytes);
    if(written!=length){
      delete response;
      request->send(503, "text/plain", "Unable to buffer shot");
      return;
    }
    request->send(response);
  });

#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  // USB BENCH ONLY. No request handler touches actuators or filesystem.
  // A single pending job prevents unbounded queue growth.
  server.on("/api/test-shot", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("seconds")) {
      request->send(400, "text/plain", "Missing seconds (5..120)");
      return;
    }
    const String raw = request->getParam("seconds")->value();
    if (raw.isEmpty() || raw.length() > 3) {
      request->send(400, "text/plain", "Invalid duration");
      return;
    }
    for (size_t i = 0; i < raw.length(); ++i) {
      if (raw[i] < '0' || raw[i] > '9') {
        request->send(400, "text/plain", "Invalid duration");
        return;
      }
    }
    const uint32_t seconds = (uint32_t)raw.toInt();
    if (seconds < 5 || seconds > 120) {
      request->send(400, "text/plain", "Duration must be 5..120 seconds");
      return;
    }
    if (!shotArchive.available()) {
      request->send(503, "text/plain", "Archive unavailable");
      return;
    }
    uint32_t expected = 0;
    if (!pendingShotTestMs.compare_exchange_strong(expected, seconds * 1000UL)) {
      request->send(409, "text/plain", "Test request already pending");
      return;
    }
    request->send(202, "text/plain", "Synthetic shot queued; refresh /api/shots shortly");
  });
#endif

#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  server.on("/api/test-shots", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("count") || !request->hasParam("seconds")) {
      request->send(400, "text/plain", "Required: count=1..100, seconds=15..120");
      return;
    }
    auto parseNumber = [](const String& value, uint32_t& out) -> bool {
      if (value.isEmpty() || value.length() > 3) return false;
      uint32_t number = 0;
      for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] < '0' || value[i] > '9') return false;
        number = number * 10 + value[i] - '0';
      }
      out = number;
      return true;
    };
    uint32_t count = 0, seconds = 0;
    if (!parseNumber(request->getParam("count")->value(), count) ||
        !parseNumber(request->getParam("seconds")->value(), seconds) ||
        count < 1 || count > 100 || seconds < 15 || seconds > 120) {
      request->send(400, "text/plain", "Invalid count or duration");
      return;
    }
    if (!shotArchive.available()) {
      request->send(503, "text/plain", "Archive unavailable");
      return;
    }
    uint8_t state = shotBatchState.load();
    while (state != 1 && state != 2) {
      if (shotBatchState.compare_exchange_weak(state, 1)) {
        pendingShotBatch.store((count << 16) | seconds);
        request->send(202, "text/plain", "Batch queued; check /api/test-shots/status");
        return;
      }
    }
    request->send(409, "text/plain", "Batch already running");
  });
  server.on("/api/test-shots/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    const uint8_t state = shotBatchState.load();
    const char* stateText = state == 1 ? "queued" : state == 2 ? "running" :
                            state == 3 ? "completed" : state == 4 ? "failed" : "idle";
    const String json = String("{\"state\":\"") + stateText +
                        "\",\"total\":" + String(shotBatchTotal.load()) +
                        ",\"queued\":" + String(shotBatchQueued.load()) +
                        ",\"saved\":" + String(shotBatchSaved.load()) +
                        ",\"failed\":" + String(shotBatchFailed.load()) + "}";
    request->send(200, "application/json", json);
  });
#endif


  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    static const char* keys[WEB_SETTING_COUNT] = {
      "targetTemp", "steamTemp", "pidKp", "pidKi", "pidKd",
      "pumpKp", "pumpKi", "pumpKd", "preinfDur", "preinfPress",
      "holdDur", "holdPress", "declineDur", "declinePress"
    };
    String output = "{";
    for (uint8_t i = 0; i < WEB_SETTING_COUNT; ++i) {
      if (i) output += ",";
      output += "\"" + String(keys[i]) + "\":" +
          String(publishedWebSettings[i].load() /
              ((i == PID_KP || i == PID_KI || i == PID_KD ||
                i == PUMP_KP || i == PUMP_KI || i == PUMP_KD ||
                i == PREINFUSE_PRESSURE || i == HOLD_PRESSURE || i == DECLINE_PRESSURE)
                  ? 10.0f : 1.0f), 1);
    }
    output += ",\"revision\":" + String(webSettingsRevision.load()) + "}";
    request->send(200, "application/json", output);
  });
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (publishedFlowActive.load()) {
      request->send(409, "text/plain", "Cannot change settings during shot");
      return;
    }
    if (!fullWebSettingsQueue) {
      request->send(503, "text/plain", "Settings queue unavailable");
      return;
    }
    static const char* keys[WEB_SETTING_COUNT] = {
      "targetTemp", "steamTemp", "pidKp", "pidKi", "pidKd",
      "pumpKp", "pumpKi", "pumpKd", "preinfDur", "preinfPress",
      "holdDur", "holdPress", "declineDur", "declinePress"
    };
    WebSettingsCommand command{};
    for (uint8_t i = 0; i < WEB_SETTING_COUNT; ++i) {
      if (!request->hasParam(keys[i], true) ||
          !parseWebSetting(request->getParam(keys[i], true)->value(), i, command.values[i])) {
        request->send(400, "text/plain", String("Invalid setting: ") + keys[i]);
        return;
      }
    }
    if (xQueueSend(fullWebSettingsQueue, &command, 0) != pdTRUE) {
      request->send(409, "text/plain", "Settings update already pending");
      return;
    }
    request->send(202, "text/plain", "Queued; reload /api/settings to confirm");
  });

  server.on("/targetTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("temp")) {
      String str = request->getParam("temp")->value();
      // Validate before converting to byte to avoid integer wraparound.
      bool valid = str.length() > 0;
      for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] < '0' || str[i] > '9') {
          valid = false;
          break;
        }
      }
      const long newTemp = valid ? str.toInt() : -1;
      if (newTemp < 0 || newTemp >= 145) {
        request->send(400, "text/plain", "Invalid target temperature");
        return;
      }
      if (!enqueueBoilerWebCommand(TARGET_TEMP, (int)newTemp)) {
        request->send(503, "text/plain", "Settings queue unavailable");
        return;
      }
      Serial.printf("Queued target temp - %ld\n", newTemp);
      request->send(202, "text/plain", "Queued");
      return;
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKp", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Kp")) {
      String str = request->getParam("Kp")->value();
      int newKp = 0;
      if (!parseBoilerPidGain(str, newKp)) {
        request->send(400, "text/plain", "Invalid PID gain");
        return;
      }
      if (!enqueueBoilerWebCommand(PID_KP, newKp)) {
        request->send(503, "text/plain", "Settings queue unavailable");
        return;
      }
      request->send(202, "text/plain", "Queued");
      return;
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Ki")) {
      String str = request->getParam("Ki")->value();
      int newKi = 0;
      if (!parseBoilerPidGain(str, newKi)) {
        request->send(400, "text/plain", "Invalid PID gain");
        return;
      }
      if (!enqueueBoilerWebCommand(PID_KI, newKi)) {
        request->send(503, "text/plain", "Settings queue unavailable");
        return;
      }
      request->send(202, "text/plain", "Queued");
      return;
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Kd")) {
      String str = request->getParam("Kd")->value();
      int newKd = 0;
      if (!parseBoilerPidGain(str, newKd)) {
        request->send(400, "text/plain", "Invalid PID gain");
        return;
      }
      if (!enqueueBoilerWebCommand(PID_KD, newKd)) {
        request->send(503, "text/plain", "Settings queue unavailable");
        return;
      }
      request->send(202, "text/plain", "Queued");
      return;
    }
    request->send_P(200, "text/plain", "OK");
  });
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

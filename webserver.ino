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

void applyPendingWebSettings() {
  if (!boilerWebCommandQueue) return;
  BoilerWebCommand command;
  // Bound loop work even under a flood of requests.
  for (int i = 0; i < 8 && xQueueReceive(boilerWebCommandQueue, &command, 0) == pdTRUE; ++i) {
    switch (command.index) {
      case TARGET_TEMP: changeTargetTemp((byte)command.value); break;
      case PID_KP: changePidKp(command.value); break;
      case PID_KI: changePidKi(command.value); break;
      case PID_KD: changePidKd(command.value); break;
      default: break;
    }
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

String processor(const String &var) {
  if (var == "TEMPERATUREC") {
    return getTemperature();
  }
  if (var == "PRESSURE") {
    return getPressure();
  }
  if (var == "TARGETTEMP") {
    return String(settings[TARGET_TEMP].value);
  }
  if (var == "PID_KP") {
    return String(boilerPid.getKp());
  }
  if (var == "PID_KI") {
    return String(boilerPid.getKi());
  }
  if (var == "PID_KD") {
    return String(boilerPid.getKd());
  }
  return String();
}

String getTemperature() {
  return String(thermoLmt.getTemperature());
}

String getPressure() {
  if (currentPressure < 0) {
    return String(0.0);
  }
  return String(currentPressure);
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
  if (wsClients > 0) {
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
      ESP.restart();
    }
    if (len == sizeof("initInfo") - 1 && memcmp(data, "initInfo", len) == 0) {
      giveInitInfoToClient(client);
    }
  }
}

void giveInitInfoToClient(AsyncWebSocketClient *client) {
  char buffer[200];
  snprintf(buffer, sizeof(buffer), "{\"name\":\"initInfo\",\"currentTemp\":\"%f\",\"currentPress\":\"%f\",\"targetTemp\":\"%u\",\"pidKp\":\"%f\",\"pidKi\":\"%f\",\"pidKd\":\"%f\"}",
          currentTemperature, currentPressure, settings[TARGET_TEMP].value, boilerPid.getKp(), boilerPid.getKi(), boilerPid.getKd());
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
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      wsClients++;
      stopWatch.setWsClients(wsClients);
      Serial.printf("Total connected clients - %u\n", wsClients);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      wsClients--;
      stopWatch.setWsClients(wsClients);
      Serial.printf("Total connected clients - %u\n", wsClients);
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebServerControllers() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
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
      changeTargetTemp((byte)newTemp);
      Serial.printf("New target temp - %ld\n", newTemp);
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKp", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Kp")) {
      String str = request->getParam("Kp")->value();
      int newKp = str.toFloat() * 10;
      changePidKp(newKp);
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Ki")) {
      String str = request->getParam("Ki")->value();
      int newKi = str.toFloat() * 10;
      changePidKi(newKi);
    }
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/changePidKd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("Kd")) {
      String str = request->getParam("Kd")->value();
      int newKd = str.toFloat() * 10;
      changePidKd(newKd);
    }
    request->send_P(200, "text/plain", "OK");
  });
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

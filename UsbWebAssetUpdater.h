#pragma once
// USB-only maintenance utility. NEVER enable on installed/mains-powered machine.
// Protocol: BEGIN <html|css|js> <bytes> <crc32hex>, DATA <hex>, END / ABORT.
// No HTTP endpoints; only the USB serial port can update whitelisted web assets.
#ifdef GAGGUINO_USB_WEB_UPDATE
#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <freertos/semphr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

class UsbWebAssetUpdater {
 public:
  // Repair an interrupted rename during startup before static handlers exist.
  static void recover() {
    const char* live[] = {"/index.html", "/css/style.css", "/js/app.js"};
    const char* backups[] = {"/index.html.bak", "/css/style.css.bak", "/js/app.js.bak"};
    for (int i = 0; i < 3; ++i) {
      if (!LittleFS.exists(live[i]) && LittleFS.exists(backups[i])) {
        if (LittleFS.rename(backups[i], live[i]))
          Serial.printf("WEBUPDATE restored %s from backup\\n", live[i]);
      }
    }
    // A leftover uncommitted candidate cannot be served.
    LittleFS.remove("/webupdate.tmp");
  }

  void poll(bool physicalFlow, SemaphoreHandle_t filesystemMutex) {
    if (active_ && physicalFlow) {
      abortUpload("physical flow active");
    }
    if (active_ && (uint32_t)(millis() - lastInputMs_) > 30000UL) {
      abortUpload("timeout");
    }
    // Limited work per loop; only allowed in a physically isolated bench build.
    for (int i = 0; i < 192 && Serial.available() > 0; ++i) {
      const char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line_[used_] = 0;
        command(line_, physicalFlow, filesystemMutex);
        used_ = 0;
      } else if (used_ < sizeof(line_) - 1) {
        line_[used_++] = c;
      } else {
        used_ = 0;
        if (active_) abortUpload("line too long");
        else Serial.println("WEBUPDATE ERR line too long");
      }
    }
  }

 private:
  char line_[180] = {};
  size_t used_ = 0;
  bool active_ = false;
  SemaphoreHandle_t heldMutex_ = nullptr;
  File output_;
  const char* target_ = nullptr;
  const char* backup_ = nullptr;
  size_t expected_ = 0;
  size_t received_ = 0;
  uint32_t expectedCrc_ = 0;
  uint32_t crc_ = 0xFFFFFFFFu;
  uint32_t lastInputMs_ = 0;
  static constexpr const char* TEMP = "/webupdate.tmp";

  static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  void release() {
    if (output_) output_.close();
    if (heldMutex_) xSemaphoreGive(heldMutex_);
    heldMutex_ = nullptr;
    target_ = nullptr;
    backup_ = nullptr;
    active_ = false;
  }
  void abortUpload(const char* why) {
    if (output_) output_.close();
    LittleFS.remove(TEMP);
    release();
    Serial.print("WEBUPDATE ERR ");
    Serial.println(why);
  }
  void command(const char* cmd, bool physicalFlow, SemaphoreHandle_t mutex) {
    if (strcmp(cmd, "ABORT") == 0) {
      if (active_) abortUpload("aborted");
      else Serial.println("WEBUPDATE OK IDLE");
      return;
    }
    if (strncmp(cmd, "BEGIN ", 6) == 0 && !active_) {
      char key[9] = {};
      unsigned long length = 0;
      unsigned long check = 0;
      char extra = 0;
      if (sscanf(cmd + 6, "%8s %lu %lx %c", key, &length, &check, &extra) != 3 ||
          length == 0 || length > 65536UL || physicalFlow || !mutex) {
        Serial.println("WEBUPDATE ERR invalid BEGIN or machine busy");
        return;
      }
      if (strcmp(key, "html") == 0) {
        target_ = "/index.html"; backup_ = "/index.html.bak";
      } else if (strcmp(key, "css") == 0) {
        target_ = "/css/style.css"; backup_ = "/css/style.css.bak";
      } else if (strcmp(key, "js") == 0) {
        target_ = "/js/app.js"; backup_ = "/js/app.js.bak";
      } else {
        Serial.println("WEBUPDATE ERR asset not allowed");
        return;
      }
      if (xSemaphoreTake(mutex, 0) != pdTRUE) {
        target_ = nullptr; backup_ = nullptr;
        Serial.println("WEBUPDATE ERR filesystem busy");
        return;
      }
      heldMutex_ = mutex;
      LittleFS.remove(TEMP);
      output_ = LittleFS.open(TEMP, "w");
      if (!output_) {
        release();
        Serial.println("WEBUPDATE ERR cannot create temporary file");
        return;
      }
      expected_ = (size_t)length;
      expectedCrc_ = (uint32_t)check;
      received_ = 0;
      crc_ = 0xFFFFFFFFu;
      lastInputMs_ = millis();
      active_ = true;
      Serial.println("WEBUPDATE OK READY");
      return;
    }
    if (!active_) {
      Serial.println("WEBUPDATE ERR not started");
      return;
    }
    lastInputMs_ = millis();
    if (strncmp(cmd, "DATA ", 5) == 0) {
      const char* hex = cmd + 5;
      const size_t characters = strlen(hex);
      if (characters == 0 || characters > 128 || (characters & 1) ||
          received_ + characters / 2 > expected_) {
        abortUpload("bad data length");
        return;
      }
      uint8_t bytes[64];
      for (size_t i = 0; i < characters / 2; ++i) {
        const int high = hexNibble(hex[2 * i]);
        const int low = hexNibble(hex[2 * i + 1]);
        if (high < 0 || low < 0) {
          abortUpload("invalid hex");
          return;
        }
        bytes[i] = (uint8_t)((high << 4) | low);
        uint32_t v = crc_ ^ bytes[i];
        for (int b = 0; b < 8; ++b) v = (v >> 1) ^ ((v & 1) ? 0xEDB88320u : 0);
        crc_ = v;
      }
      const size_t size = characters / 2;
      if (output_.write(bytes, size) != size) {
        abortUpload("flash write failed");
        return;
      }
      received_ += size;
      Serial.println("WEBUPDATE OK DATA");
      return;
    }
    if (strcmp(cmd, "END") == 0) {
      if (received_ != expected_ || (crc_ ^ 0xFFFFFFFFu) != expectedCrc_) {
        abortUpload("size or CRC32 mismatch");
        return;
      }
      output_.flush();
      output_.close();
      // Keep a rollback copy of the immediately previous version.
      LittleFS.remove(backup_);
      const bool hadOld = LittleFS.exists(target_);
      if (hadOld && !LittleFS.rename(target_, backup_)) {
        abortUpload("backup rename failed");
        return;
      }
      if (!LittleFS.rename(TEMP, target_)) {
        if (hadOld) LittleFS.rename(backup_, target_);
        abortUpload("install rename failed");
        return;
      }
      release();
      Serial.println("WEBUPDATE OK COMMIT");
      return;
    }
    abortUpload("unknown command");
  }
};
#endif

// Lightweight shot recorder. Nothing is written to flash from loop().
// Dedicated worker commits completed captures into LittleFS.
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace ShotArchive {
constexpr uint32_t MAGIC = 0x544F4853; // "SHOT" little-endian
constexpr uint16_t VERSION = 1;
constexpr uint16_t MAX_SAMPLES = 1500; // 5 Hz * 300 s maximum
constexpr uint16_t MAX_SHOTS = 60;
constexpr uint32_t MIN_DURATION_MS = 15000;
constexpr uint32_t SAMPLE_MS = 200;

struct Sample {
  uint32_t elapsedMs;
  int16_t tempTenths;
  uint16_t pressureHundredths;
};
struct FileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint32_t durationMs;
  uint32_t reserved; // reserved for future reliable UTC timestamp
};
struct Capture {
  FileHeader header;
  Sample samples[MAX_SAMPLES];
};
static_assert(sizeof(Sample) == 8, "Shot sample must be eight bytes");
static_assert(sizeof(FileHeader) == 16, "Shot header must be sixteen bytes");

static QueueHandle_t completed = nullptr;
static QueueHandle_t available = nullptr;
static SemaphoreHandle_t fsMutex = nullptr;
static Capture* active = nullptr;
static bool enabled = false;
static bool wasFlow = false;
static uint32_t beganMs = 0;
static uint32_t nextSampleMs = 0;
static uint32_t nextId = 1;

static bool parseShotId(const String& path, uint32_t& id) {
  const int start = path.lastIndexOf('/') + 1;
  if (path.length() != (unsigned)(start + 13) || path.charAt(start) != 's' ||
      !path.endsWith(".bin")) return false;
  uint32_t result = 0;
  for (int i = 1; i <= 8; ++i) {
    const char c = path.charAt(start + i);
    if (c < '0' || c > '9') return false;
    result = result * 10 + c - '0';
  }
  id = result;
  return true;
}
static String filename(uint32_t id) {
  char path[32];
  snprintf(path, sizeof(path), "/shots/s%08lu.bin", (unsigned long)id);
  return String(path);
}
static uint16_t listShotIds(uint32_t* ids, uint16_t capacity) {
  uint16_t count = 0;
  File dir = LittleFS.open("/shots");
  if (!dir || !dir.isDirectory()) return 0;
  File file = dir.openNextFile();
  while (file) {
    uint32_t id;
    if (parseShotId(String(file.name()), id)) {
      if (count < capacity) ids[count++] = id;
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  // Oldest to newest; only 60 files plus at most one freshly committed file.
  for (uint16_t i = 1; i < count; ++i) {
    const uint32_t x = ids[i];
    int j = i - 1;
    while (j >= 0 && ids[j] > x) { ids[j + 1] = ids[j]; --j; }
    ids[j + 1] = x;
  }
  return count;
}
static void prune() {
  uint32_t ids[MAX_SHOTS + 4];
  uint16_t count = listShotIds(ids, MAX_SHOTS + 4);
  for (uint16_t i = 0; i < count - MAX_SHOTS && count > MAX_SHOTS; ++i) {
    LittleFS.remove(filename(ids[i]));
  }
}
static void storeCapture(const Capture* capture) {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(1500)) != pdTRUE) {
    Serial.println("SHOT_ARCHIVE: filesystem busy; shot discarded");
    return;
  }
  const String finalPath = filename(nextId);
  const String temporaryPath = "/shots/pending.tmp";
  LittleFS.remove(temporaryPath);
  File out = LittleFS.open(temporaryPath, "w");
  bool ok = (bool)out;
  if (ok) {
    const size_t headerBytes = out.write((const uint8_t*)&capture->header, sizeof(FileHeader));
    const size_t sampleBytes = out.write((const uint8_t*)capture->samples, capture->header.count * sizeof(Sample));
    out.flush();
    ok = headerBytes == sizeof(FileHeader) && sampleBytes == capture->header.count * sizeof(Sample);
    out.close();
  }
  if (ok) ok = LittleFS.rename(temporaryPath, finalPath);
  if (ok) {
    ++nextId;
    prune();
    Serial.printf("SHOT_ARCHIVE: saved %s (%u samples)\n", finalPath.c_str(), capture->header.count);
  } else {
    LittleFS.remove(temporaryPath);
    Serial.println("SHOT_ARCHIVE: cannot save shot");
  }
  xSemaphoreGive(fsMutex);
}
static void worker(void*) {
  Capture* capture = nullptr;
  for (;;) {
    if (xQueueReceive(completed, &capture, portMAX_DELAY) != pdTRUE) continue;
    if (capture) {
      storeCapture(capture);
      xQueueSend(available, &capture, portMAX_DELAY);
    }
  }
}
} // namespace ShotArchive

void initShotArchive() {
  using namespace ShotArchive;
  if (!LittleFS.exists("/index.html")) {
    Serial.println("SHOT_ARCHIVE: LittleFS unavailable; recording disabled");
    return;
  }
  if (!LittleFS.exists("/shots") && !LittleFS.mkdir("/shots")) {
    Serial.println("SHOT_ARCHIVE: cannot create /shots");
    return;
  }
  LittleFS.remove("/shots/pending.tmp"); // discard power-loss partial file
  uint32_t ids[MAX_SHOTS + 4];
  const uint16_t count = listShotIds(ids, MAX_SHOTS + 4);
  if (count) nextId = ids[count - 1] + 1;
  fsMutex = xSemaphoreCreateMutex();
  completed = xQueueCreate(2, sizeof(Capture*));
  available = xQueueCreate(2, sizeof(Capture*));
  Capture* first = (Capture*)malloc(sizeof(Capture));
  Capture* second = (Capture*)malloc(sizeof(Capture));
  if (!fsMutex || !completed || !available || !first || !second) {
    free(first); free(second);
    Serial.println("SHOT_ARCHIVE: no memory; recording disabled");
    return;
  }
  active = first;
  xQueueSend(available, &second, 0);
  TaskHandle_t task = nullptr;
  if (xTaskCreate(worker, "shotArchive", 4096, nullptr, 1, &task) != pdPASS) {
    Serial.println("SHOT_ARCHIVE: cannot start writer task");
    return;
  }
  enabled = true;
  Serial.println("SHOT_ARCHIVE: ready (5 Hz, >= 15 s, last 60 shots)");
}

void sampleShotArchive() {
  using namespace ShotArchive;
  if (!enabled) return;
  const bool flowing = stopWatch.getIsFlow();
  const uint32_t now = millis();
  if (flowing && !wasFlow) {
    if (!active) xQueueReceive(available, &active, 0);
    beganMs = now;
    nextSampleMs = now;
    if (active) {
      active->header.magic = MAGIC;
      active->header.version = VERSION;
      active->header.count = 0;
      active->header.durationMs = 0;
      active->header.reserved = 0;
    }
  }
  if (flowing && active && (int32_t)(now - nextSampleMs) >= 0) {
    nextSampleMs = now + SAMPLE_MS;
    if (active->header.count < MAX_SAMPLES) {
      Sample& sample = active->samples[active->header.count++];
      sample.elapsedMs = now - beganMs;
      sample.tempTenths = (int16_t)constrain((int)(currentTemperature * 10.f), -500, 1450);
      sample.pressureHundredths = (uint16_t)constrain((int)(currentPressure * 100.f), 0, 1200);
    }
  }
  if (!flowing && wasFlow && active) {
    // This is the completed shot duration, NOT the after-shot currentTime.
    const uint32_t duration = stopWatch.getTimerTime();
    if (duration >= MIN_DURATION_MS && active->header.count > 0) {
      active->header.durationMs = duration;
      Capture* finished = active;
      if (xQueueSend(completed, &finished, 0) == pdTRUE) {
        active = nullptr;
        xQueueReceive(available, &active, 0);
      } else {
        Serial.println("SHOT_ARCHIVE: writer backlog; shot discarded");
      }
    }
  }
  wasFlow = flowing;
}

void initShotArchiveRoutes() {
  using namespace ShotArchive;
  server.on("/api/shots", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!enabled || xSemaphoreTake(fsMutex, 0) != pdTRUE) {
      request->send(503, "application/json", "{\"error\":\"archive unavailable\"}");
      return;
    }
    uint32_t ids[MAX_SHOTS + 4];
    const uint16_t count = listShotIds(ids, MAX_SHOTS + 4);
    String json = "[";
    for (int i = (int)count - 1; i >= 0; --i) {
      File in = LittleFS.open(filename(ids[i]), "r");
      FileHeader header{};
      const bool good = in && in.readBytes((char*)&header, sizeof(header)) == sizeof(header) &&
                        header.magic == MAGIC && header.version == VERSION &&
                        header.count <= MAX_SAMPLES;
      if (in) in.close();
      if (!good) continue;
      if (json.length() > 1) json += ",";
      json += "{\"id\":" + String(ids[i]) + ",\"durationMs\":" + String(header.durationMs) +
              ",\"count\":" + String(header.count) + "}";
    }
    json += "]";
    xSemaphoreGive(fsMutex);
    request->send(200, "application/json", json);
  });

  server.on("/api/shot", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("id")) {
      request->send(400, "text/plain", "Missing shot id");
      return;
    }
    const String raw = request->getParam("id")->value();
    if (raw.length() == 0 || raw.length() > 8) {
      request->send(400, "text/plain", "Invalid shot id");
      return;
    }
    uint32_t id = 0;
    for (size_t i = 0; i < raw.length(); ++i) {
      if (raw[i] < '0' || raw[i] > '9') {
        request->send(400, "text/plain", "Invalid shot id");
        return;
      }
      id = id * 10 + raw[i] - '0';
    }
    const String path = filename(id);
    if (!enabled || !LittleFS.exists(path)) {
      request->send(404, "text/plain", "Shot not found");
      return;
    }
    request->send(LittleFS, path, "application/octet-stream");
  });
}

#pragma once
// Local shot archive. All flash I/O happens outside the PID loop.
// Format v1: little-endian header (16 bytes) + 8-byte samples.
// Do not change these packed structs without updating data/js/app.js.
#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <atomic>
#include <time.h>

class ShotArchive {
 public:
  static constexpr uint16_t MAX_SAMPLES = 600; // 120 seconds at 5 Hz
  static constexpr uint8_t MAX_SHOTS = 60;
  static constexpr uint32_t MIN_DURATION_MS = 15000;
  static constexpr uint32_t SAMPLE_INTERVAL_MS = 200;
  static constexpr uint32_t MAGIC = 0x31485347; // GSH1 in little endian
  struct __attribute__((packed)) Header {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint32_t durationMs;
    uint32_t startedUnix; // UTC seconds; zero when NTP clock is unavailable
  };
  struct __attribute__((packed)) Sample {
    uint16_t tick;         // 200ms steps from shot start
    int16_t temperature10; // 0.1 degrees C
    uint16_t pressure100;  // 0.01 bar
    uint8_t heatPercent;
    uint8_t pumpPercent;
  };
  static_assert(sizeof(Header)==16, "Shot header must be 16 bytes");
  static_assert(sizeof(Sample)==8, "Shot sample must be 8 bytes");

  void begin() {
    if (!LittleFS.exists("/index.html")) {
      Serial.println("Shot archive unavailable: LittleFS is not mounted");
      return;
    }
    mutex_ = xSemaphoreCreateMutex();
    pending_ = xQueueCreate(2, sizeof(uint8_t));
    if (!mutex_ || !pending_) {
      Serial.println("Shot archive unavailable: FreeRTOS allocation failure");
      return;
    }
    if (!LittleFS.exists("/shots") && !LittleFS.mkdir("/shots")) {
      Serial.println("Shot archive unavailable: mkdir failed");
      return;
    }
    // Read the largest file number so identifiers survive normal reboots.
    File dir = LittleFS.open("/shots");
    if (dir && dir.isDirectory()) {
      File file = dir.openNextFile();
      while (file) {
        uint32_t id = 0;
        const char *name = strrchr(file.name(), '/');
        name = name ? name + 1 : file.name();
        if (sscanf(name, "%8lu.bin", &id)==1 && id>nextId_) nextId_=id;
        file = dir.openNextFile();
      }
    }
    BaseType_t taskOk = xTaskCreate(writerEntry, "ShotArchive", 6144, this, 1, &writer_);
    ready_ = taskOk == pdPASS;
    Serial.printf("Shot archive %s; latest id=%lu\n", ready_?"ready":"task creation failed", (unsigned long)nextId_);
  }

  void sample(bool flowing, uint32_t durationMs, float temperature, float pressure,
              int heatPercent, int pumpPercent) {
    if (!ready_) return;
    if (flowing && !wasFlowing_) {
      capturing_ = -1;
      for (int i=0; i<2; ++i) {
        if (slots_[i].state==0) {
          capturing_=i;
          slots_[i].state=1;
          slots_[i].header={MAGIC,1,0,0,validUtcNow()};
          break;
        }
      }
      if (capturing_<0) Serial.println("Shot archive busy: capture skipped");
    }
    if (flowing && capturing_>=0) {
      Buffer &buf=slots_[capturing_];
      uint16_t tick = (uint16_t)min(durationMs/SAMPLE_INTERVAL_MS, (uint32_t)UINT16_MAX);
      if (buf.header.count==0 || tick > buf.samples[buf.header.count-1].tick) {
        if (buf.header.count<MAX_SAMPLES) {
          const int16_t temp10=(int16_t)constrain((int)lroundf(isfinite(temperature)?temperature*10.f:-500.f),-32768,32767);
          const uint16_t press100=(uint16_t)constrain((int)lroundf(isfinite(pressure)?pressure*100.f:0.f),0,65535);
          buf.samples[buf.header.count++]={tick,temp10,press100,
            (uint8_t)constrain(heatPercent,0,100),(uint8_t)constrain(pumpPercent,0,100)};
        }
      }
    }
    if (!flowing && wasFlowing_ && capturing_>=0) {
      Buffer &buf=slots_[capturing_];
      buf.header.durationMs=durationMs; // StopWatch retains last completed duration
      const uint8_t slot=(uint8_t)capturing_;
      if (durationMs>=MIN_DURATION_MS && buf.header.count>0) {
        buf.state=2;
        if (xQueueSend(pending_,&slot,0)!=pdTRUE) {
          buf.state=0;
          Serial.println("Shot archive queue full: record dropped");
        }
      } else {
        buf.state=0;
      }
      capturing_=-1;
    }
    wasFlowing_=flowing;
  }

#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  // BENCH ONLY: synthesize one completed shot without touching the stopwatch,
  // pump, heater, sensor values or any machine actuator. Called by loop().
  // Flash writes still happen exclusively on the existing writer task.
  bool generateTestShot(uint32_t durationMs) {
    if (!ready_ || durationMs < 5000 || durationMs > 120000 ||
        wasFlowing_ || capturing_ >= 0) return false;
    int slot = -1;
    for (int i = 0; i < 2; ++i) {
      if (slots_[i].state == 0) { slot = i; break; }
    }
    if (slot < 0) return false;
    Buffer &buf = slots_[slot];
    buf.state = 1;
    buf.synthetic = false;
    buf.header = {MAGIC, 1, 0, durationMs, validUtcNow()};
    const uint16_t count = (uint16_t)min(durationMs / SAMPLE_INTERVAL_MS,
                                          (uint32_t)MAX_SAMPLES);
    for (uint16_t i = 0; i < count; ++i) {
      const float t = i * 0.2f;
      const float progress = t / (durationMs / 1000.0f);
      const float pressure = progress < 0.2f ? 1.5f * progress / 0.2f :
          progress < 0.45f ? 1.5f + 7.5f * (progress - 0.2f) / 0.25f :
          progress < 0.8f ? 9.0f : 9.0f * (1.0f - progress) / 0.2f;
      const float temp = 93.0f + 0.5f * sinf(t / 4.0f);
      buf.samples[i] = {
        i,
        (int16_t)lroundf(temp * 10.0f),
        (uint16_t)constrain((int)lroundf(pressure * 100.0f), 0, 1200),
        (uint8_t)constrain((int)lroundf(45.0f + 15.0f * sinf(t / 5.0f)), 0, 100),
        (uint8_t)70
      };
    }
    buf.header.count = count;
    if (durationMs < MIN_DURATION_MS) {
      buf.state = 0;
      Serial.printf("SHOT_TEST: rejected %lu ms (< 15000 ms)\n",
                    (unsigned long)durationMs);
      return true;
    }
    const uint8_t index = (uint8_t)slot;
    buf.synthetic = true;
    buf.state = 2;
    if (xQueueSend(pending_, &index, 0) != pdTRUE) {
      buf.state = 0;
      return false;
    }
    Serial.printf("SHOT_TEST: queued %lu ms, %u samples\n",
                  (unsigned long)durationMs, count);
    return true;
  }
#endif

#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  uint32_t testSaved() const { return testSaved_.load(); }
  uint32_t testFailed() const { return testFailed_.load(); }
#endif

  bool available() const { return ready_; }

  // Web handlers only: skip if the background writer is using LittleFS.
  bool list(String &output) {
    if (!ready_ || xSemaphoreTake(mutex_,0)!=pdTRUE) return false;
    output="[";
    File dir=LittleFS.open("/shots");
    bool first=true;
    if (dir && dir.isDirectory()) {
      File file=dir.openNextFile();
      while(file) {
        const String name=String(file.name());
        uint32_t id=0;
        const char *shortName=strrchr(file.name(),'/');
        shortName=shortName?shortName+1:file.name();
        if (sscanf(shortName,"%8lu.bin",&id)==1 && file.size()>=sizeof(Header)) {
          Header h{};
          if (file.read((uint8_t*)&h,sizeof(h))==sizeof(h) &&
              h.magic==MAGIC && h.version==1 &&
              h.count<=MAX_SAMPLES && file.size()==sizeof(Header)+(size_t)h.count*sizeof(Sample)) {
            if(!first)output+=",";
            first=false;
            output+="{\"id\":"+String(id)+",\"durationMs\":"+String(h.durationMs)+
                    ",\"samples\":"+String(h.count)+",\"startedUnix\":"+String(h.startedUnix)+"}";
          }
        }
        file=dir.openNextFile();
      }
    }
    output+="]";
    xSemaphoreGive(mutex_);
    return true;
  }

  // Returns a bounded binary blob. Caller must send it after releasing lock.
  bool read(uint32_t id, uint8_t *&data, size_t &length) {
    data=nullptr;length=0;
    if (!ready_ || id==0 || xSemaphoreTake(mutex_,0)!=pdTRUE) return false;
    char path[40];snprintf(path,sizeof(path),"/shots/%08lu.bin",(unsigned long)id);
    File file=LittleFS.open(path,"r");
    bool ok=false;
    if (file && file.size()>=sizeof(Header) &&
        file.size()<=sizeof(Header)+MAX_SAMPLES*sizeof(Sample)) {
      length=file.size();
      data=(uint8_t*)malloc(length);
      if (data)ok=file.read(data,length)==length;
    }
    if(file)file.close();
    xSemaphoreGive(mutex_);
    if(!ok){free(data);data=nullptr;length=0;}
    return ok;
  }

 private:
  // time() is maintained by ESP32 SNTP independently of the local display
  // clock. Refuse uninitialized time rather than persist a 1970 timestamp.
  static uint32_t validUtcNow() {
    const time_t now = time(nullptr);
    return now >= (time_t)1577836800 && (uint64_t)now <= UINT32_MAX
        ? (uint32_t)now : 0;
  }

  struct Buffer {
    volatile uint8_t state=0; // 0=free; 1=capturing; 2=queued/writing
#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
    bool synthetic=false;
#endif
    Header header{};
    Sample samples[MAX_SAMPLES]{};
  };
  Buffer slots_[2];
  volatile bool ready_=false;
  bool wasFlowing_=false;
  int8_t capturing_=-1;
  uint32_t nextId_=0;
  QueueHandle_t pending_=nullptr;
  SemaphoreHandle_t mutex_=nullptr;
  TaskHandle_t writer_=nullptr;
#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
  std::atomic<uint32_t> testSaved_{0};
  std::atomic<uint32_t> testFailed_{0};
#endif

  static void writerEntry(void *arg) {
    static_cast<ShotArchive*>(arg)->writerLoop();
  }
  void writerLoop() {
    uint8_t slot;
    for (;;) {
      if (xQueueReceive(pending_,&slot,portMAX_DELAY)!=pdTRUE) continue;
      bool ok=false;
      if (xSemaphoreTake(mutex_,portMAX_DELAY)==pdTRUE) {
        Buffer &buf=slots_[slot];
        if (buf.state==2) {
          // Temporary file is ignored by listing if power is lost mid-write.
          const char *tmp="/shots/pending.tmp";
          LittleFS.remove(tmp);
          File file=LittleFS.open(tmp,"w");
          if (file) {
            const size_t dataBytes=(size_t)buf.header.count*sizeof(Sample);
            bool written=file.write((const uint8_t*)&buf.header,sizeof(Header))==sizeof(Header);
            if(written)written=file.write((const uint8_t*)buf.samples,dataBytes)==dataBytes;
            file.flush();
            file.close();
            if(written){
              char path[40];
              snprintf(path,sizeof(path),"/shots/%08lu.bin",(unsigned long)(nextId_+1));
              if(LittleFS.rename(tmp,path)){++nextId_;ok=true;prune();}
            }
          }
          if(!ok)LittleFS.remove(tmp);
        }
        xSemaphoreGive(mutex_);
      }
#ifdef GAGGUINO_SHOT_ARCHIVE_TEST
      if (slots_[slot].synthetic) {
        if (ok) testSaved_.fetch_add(1);
        else testFailed_.fetch_add(1);
        slots_[slot].synthetic = false;
      }
#endif
      slots_[slot].state=0;
      if(!ok)Serial.println("Shot archive: flash write failed");
    }
  }
  void prune() {
    // Called only while holding the LittleFS mutex.
    for(;;) {
      File dir=LittleFS.open("/shots");
      uint32_t oldest=UINT32_MAX;
      uint16_t count=0;
      if(dir&&dir.isDirectory()){
        File file=dir.openNextFile();
        while(file){
          uint32_t id=0;const char *name=strrchr(file.name(),'/');
          name=name?name+1:file.name();
          if(sscanf(name,"%8lu.bin",&id)==1 && id>0){
            ++count;if(id<oldest)oldest=id;
          }
          file=dir.openNextFile();
        }
      }
      if(count<=MAX_SHOTS || oldest==UINT32_MAX)break;
      char path[40];snprintf(path,sizeof(path),"/shots/%08lu.bin",(unsigned long)oldest);
      if(!LittleFS.remove(path))break;
    }
  }
};

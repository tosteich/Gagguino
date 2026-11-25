#define maxRecords 30

RTC_DATA_ATTR int loggerCounter = 0;

static char currDate[24];

void updateCurrDate() {
  timeClient.update();
  const time_t timeNow = static_cast<time_t>(timeClient.getEpochTime());
  struct tm date = {};
  if (gmtime_r(&timeNow, &date) == nullptr) {
    currDate[0] = '\0';
    return;
  }
  snprintf(currDate, sizeof(currDate), "%04d_%02d_%02d.txt",
           date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
}

typedef struct {
  float Temp;
  float Press;
  int BoilPw;
  unsigned long Time;
} sensorReadings;

RTC_DATA_ATTR sensorReadings Readings[maxRecords];

void updateLog() {
  timeClient.update();
  unsigned long timeNow = timeClient.getEpochTime();
  // RTC-retained state can outlive the current firmware and may be invalid.
  if (loggerCounter < 0 || loggerCounter >= maxRecords) loggerCounter = 0;
  Readings[loggerCounter].Temp = currentTemperature;
  Readings[loggerCounter].Press = currentPressure;
  Readings[loggerCounter].BoilPw = heatPower;
  Readings[loggerCounter].Time = timeNow;
  loggerCounter++;

  if (loggerCounter >= maxRecords) {
    if (WiFi.status() == WL_CONNECTED) {
      ftp.OpenConnection();
      ftp.ChangeWorkDir("/ESD-USB/Gaggia");
      ftp.InitFile("Type A");
      ftp.AppendFile(currDate);
      char buffer[40 * maxRecords];
      int j = 0;
      for (int i = 0; i < maxRecords; i++) {
        j += sprintf(buffer + j, "%f,%f,%u,%lu\n", Readings[i].Temp, Readings[i].Press, Readings[i].BoilPw, Readings[i].Time);
      }
      ftp.Write(buffer);
      ftp.CloseFile();
      ftp.CloseConnection();
    }
    loggerCounter = 0;
  }
}
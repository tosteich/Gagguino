#include "LogFormatter.h"
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
      // Build and validate the complete payload before opening FTP.
      // Each of the 30 rows has up to 96 bytes reserved; no unchecked writes.
      char buffer[96 * maxRecords];
      if (currDate[0] == '\0' || !formatLogRecords(Readings, maxRecords, buffer, sizeof(buffer))) {
        Serial.println("ERROR: log payload/date invalid; skipping FTP write");
      } else {
        ftp.OpenConnection();
        ftp.ChangeWorkDir("/ESD-USB/Gaggia");
        ftp.InitFile("Type A");
        ftp.AppendFile(currDate);
        ftp.Write(buffer);
        ftp.CloseFile();
        ftp.CloseConnection();
      }
    }
    loggerCounter = 0;
  }
}
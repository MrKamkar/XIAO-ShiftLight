#include "data_logger.h"
#include "config.h"
#include <LittleFS.h>

QueueHandle_t logQueue = NULL;

// Inicjalizacja systemu plików LittleFS
void initDataLogger() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }
}

// Nadpisanie pliku z telemetrią i rozpoczęcie zapisu
void startDataLog() {
  File file = LittleFS.open("/telemetry.csv", "w");
  if(file) {
    file.println("Time(ms),RPM,Speed(km/h),Temp(C),Load(%),Volt(V),IAT(C),TPS(%),MAP(kPa),Fuel(%)");
    file.close();
    isLogging = true;
  }
}

void stopDataLog() {
  isLogging = false; 
}

// Zadanie logowania - działa w tle, aby nie blokować CAN i WiFi
void taskLogging(void *pvParameters) {
  TelemetryData data;
  while (true) {
    // Czekaj na dane w kolejce (blokuje zadanie, gdy nie ma nic do zapisu)
    if (xQueueReceive(logQueue, &data, portMAX_DELAY) == pdPASS) {
      if (isLogging) {
        writeTelemetryToFlash(data);
      }
    }
  }
}

// Fizyczny zapis do pamięci Flash
void writeTelemetryToFlash(const TelemetryData& data) {
  File file = LittleFS.open("/telemetry.csv", "a");
  if (file) {
    char csvLine[128];
    snprintf(csvLine, sizeof(csvLine), "%lu,%d,%d,%d,%d,%.2f,%d,%d,%d,%d\n", 
             data.timestamp, data.rpm, data.speed, data.temp, data.load, 
             data.volt, data.iat, data.tps, data.map, data.fuel);
    file.print(csvLine);
    file.close();
  }
}

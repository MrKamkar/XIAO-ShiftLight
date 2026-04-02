#include "data_logger.h"
#include "config.h"
#include <LittleFS.h>

QueueHandle_t logQueue = NULL;

// Inicjalizacja systemu plików LittleFS
void initDataLogger() {
  if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
    LittleFS.begin(true);
    if (LittleFS.exists("/telemetry.csv")) LittleFS.remove("/telemetry.csv");
    xSemaphoreGive(fsMutex);
  }
}

// Rozpoczęcie sesji nagrywania (dopisanie do pliku)
void startDataLog() {
  isLogging = true;
}

void stopDataLog() {
  isLogging = false; 
}

// Zadanie logowania - działa w tle, aby nie blokować CAN i BLE
void taskLogging(void *pvParameters) {
  TelemetryData data;
  File file;
  uint32_t lastFlush = 0;
  uint16_t checkCounter = 0;
  bool isSpaceFull = false;
  bool lastIsLogging = false;

  while (true) {
    // Czekaj na dane w kolejce (blokuje zadanie, gdy nie ma nic do zapisu)
    if (xQueueReceive(logQueue, &data, portMAX_DELAY) == pdPASS) {
      if (isLogging) {
        if (!lastIsLogging) {
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            if (file) file.close();
            file = LittleFS.open("/telemetry.bin", "a"); 
            xSemaphoreGive(fsMutex);
          }
          lastIsLogging = true;
        } else if (!file) {
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            file = LittleFS.open("/telemetry.bin", "a");
            xSemaphoreGive(fsMutex);
          }
        }

        // Usuwamy teoretyczne sprawdzanie wolnego miejsca (v3.1) - nagrywamy dopóki write() zwraca sukces

        if (file) {
          // Zapis binarny (raw struct dump)
          size_t bytesWritten = 0;
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            bytesWritten = file.write((const uint8_t*)&data, sizeof(TelemetryData));
            currentFileSize = file.size(); 
            
            if (millis() - lastFlush > 5000) {
              file.flush();
              lastFlush = millis();
            }
            xSemaphoreGive(fsMutex);
          }

          // Jeśli fizycznie zabrakło miejsca (write() nie zapisał całego rekordu)
          if (bytesWritten < sizeof(TelemetryData)) {
            isLogging = false; 
            if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
               file.close(); 
               xSemaphoreGive(fsMutex);
            }
            continue; 
          }
        }
      } else {
        if (file) {
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            file.close();
            xSemaphoreGive(fsMutex);
          }
        }
        lastIsLogging = false; 
      }
    }
  }
}

void wipeFilesystem() {
  isLogging = false; 
  if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
    LittleFS.format();
    LittleFS.begin(true);
    xSemaphoreGive(fsMutex);
  }
}


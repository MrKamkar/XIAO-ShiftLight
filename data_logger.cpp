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

        if (file) {
          // Sprawdzenie wolnego miejsca (co 100 rekordów, by nie przeciążać LittleFS)
          if (checkCounter++ >= 100) {
            checkCounter = 0;
            size_t total = 0, used = 0;
            if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
               total = LittleFS.totalBytes();
               used = LittleFS.usedBytes();
               xSemaphoreGive(fsMutex);
            }
            if (total - used < 10240) isSpaceFull = true; 
          }

          if (isSpaceFull) { 
            isLogging = false; 
            if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
               file.close(); 
               xSemaphoreGive(fsMutex);
            }
            isSpaceFull = false; 
            continue; 
          }

          // Zapis binarny (raw struct dump)
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            file.write((const uint8_t*)&data, sizeof(TelemetryData));
            currentFileSize = file.size(); // Aktualizacja liczniku "live"
            
            // Flush co 5s (optymalizacja żywotności Flash, przy zachowaniu bezpieczeństwa danych)
            if (millis() - lastFlush > 5000) {
              file.flush();
              lastFlush = millis();
            }
            xSemaphoreGive(fsMutex);
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


#include "data_logger.h"
#include "config.h"
#include <LittleFS.h>

QueueHandle_t logQueue = NULL;

// Inicjalizacja systemu plików LittleFS
void initDataLogger() {
  LittleFS.begin(true);
  
  // Usuwa stary plik CSV, jeśli istnieje (zastąpiony przez .bin)
  if (LittleFS.exists("/telemetry.csv")) {
    LittleFS.remove("/telemetry.csv");
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

  while (true) {
    // Czekaj na dane w kolejce (blokuje zadanie, gdy nie ma nic do zapisu)
    if (xQueueReceive(logQueue, &data, portMAX_DELAY) == pdPASS) {
      if (isLogging) {
        // Otwórz plik tylko raz (trzymaj otwarty dopóki logowanie trwa)
        if (!file) file = LittleFS.open("/telemetry.bin", "a");
        if (file) {
          // Sprawdzenie wolnego miejsca (co 100 rekordów, by nie przeciążać LittleFS)
          if (checkCounter++ >= 100) {
            checkCounter = 0;
            size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
            if (freeSpace < 102400) isSpaceFull = true; 
          }

          if (isSpaceFull) { 
            isLogging = false; 
            file.close(); 
            isSpaceFull = false; 
            continue; 
          }

          // Zapis binarny (raw struct dump)
          file.write((const uint8_t*)&data, sizeof(TelemetryData));
          
          // Flush co 5s (optymalizacja żywotności Flash, przy zachowaniu bezpieczeństwa danych)
          if (millis() - lastFlush > 5000) {
            file.flush();
            lastFlush = millis();
          }
        }
      } else if (file) {
        file.close(); 
        file = File(); // Reset obiektu pliku, by !file zadziałało przy nowym REC
      }
    }
  }
}

void wipeFilesystem() {
  isLogging = false; 
  LittleFS.format();
  LittleFS.begin(true);
}


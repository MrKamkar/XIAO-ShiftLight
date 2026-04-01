#include "data_logger.h"
#include "config.h"
#include <LittleFS.h>

QueueHandle_t logQueue = NULL;

// Inicjalizacja systemu plików LittleFS
void initDataLogger() {
  LittleFS.begin(true); // Parametr 'true' automatycznie formatuje przy pierwszym uruchomieniu
}

// Nadpisanie pliku z telemetrią i rozpoczęcie zapisu
void startDataLog() {
  File file = LittleFS.open("/telemetry.csv", "w");
  if(file) {
    file.println("Time(ms),RPM,Speed(km/h),Temp(C),Load(%),Volt(V),IAT(C),TPS(%),MAP(kPa),Fuel(%),G(g)");
    file.close();
    isLogging = true;
  }
}

void stopDataLog() {
  isLogging = false; 
}

// Zadanie logowania - działa w tle, aby nie blokować CAN i BLE
void taskLogging(void *pvParameters) {
  TelemetryData data;
  File file;
  uint32_t lastFlush = 0;

  while (true) {
    // Czekaj na dane w kolejce (blokuje zadanie, gdy nie ma nic do zapisu)
    if (xQueueReceive(logQueue, &data, portMAX_DELAY) == pdPASS) {
      if (isLogging) {
        // Otwórz plik tylko raz (trzymaj otwarty dopóki logowanie trwa)
        if (!file) file = LittleFS.open("/telemetry.csv", "a");
        if (file) {
          char csvLine[140];
          snprintf(csvLine, sizeof(csvLine), "%lu,%d,%d,%d,%d,%.2f,%d,%d,%d,%d,%.2f\n", 
                   data.timestamp, data.rpm, data.speed, data.temp, data.load, 
                   data.volt, data.iat, data.tps, data.map, data.fuel, data.gforce);
          file.print(csvLine);
          
          // Flush co 1s (zabezpieczenie przed utratą danych przy nagłym odcięciu zasilania)
          if (millis() - lastFlush > 1000) {
            file.flush();
            lastFlush = millis();
          }
        }
      } else if (file) {
        file.close(); // Zamknij plik gdy logowanie zostało wyłączone
      }
    }
  }
}

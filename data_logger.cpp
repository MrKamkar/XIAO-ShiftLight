#include "data_logger.h"
#include "config.h"
#include <LittleFS.h>

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

// Zatrzymanie zapisu danych
void stopDataLog() {
  isLogging = false; 
}

// Zapis danych do pliku
void logTelemetryData() {
  if (!isLogging) return;
  
  File file = LittleFS.open("/telemetry.csv", "a"); // Tryb dopisywania danych
  if (file) {
    char csvLine[128]; // Bufor dla wszystkich parametrów
    snprintf(csvLine, sizeof(csvLine), "%lu,%d,%d,%d,%d,%.2f,%d,%d,%d,%d\n", 
             millis(), currentRPM, currentSpeed, currentTemp, currentLoad, 
             currentVolt, currentIAT, currentTPS, currentMAP, currentFuel);
    file.print(csvLine);
    file.close();
  }
}

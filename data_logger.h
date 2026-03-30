#pragma once
#include <Arduino.h>

// Inicjalizacja systemu plików i start nowego logu
void initDataLogger();

// Czyści stary arkusz i inicjuje nagłówki CSV dla nowego nagrania
void startDataLog();

// Zamyka cykl zapisu bezpiecznika isLogging
void stopDataLog();

// Zapisuje jeden wiersz danych do pliku CSV
void logTelemetryData();

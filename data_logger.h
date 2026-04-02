#pragma once
#include <Arduino.h>

// System logowania i kolejkowania danych
struct __attribute__((packed)) TelemetryData {
  uint32_t timestamp;
  int32_t rpm;
  int32_t speed;
  int32_t temp;
  int32_t load;
  float volt;
  int32_t iat;
  int32_t tps;
  int32_t map;
  int32_t fuel;
  float gforce;
};

// Inicjalizacja systemu plików i start nowego logu
void initDataLogger();

// Czyści stary arkusz i inicjuje nagłówki CSV dla nowego nagrania
void startDataLog();

// Zamyka cykl zapisu bezpiecznika isLogging
void stopDataLog();

// Zadanie asynchronicznego zapisu danych (FreeRTOS)
void taskLogging(void *pvParameters);

// Czyści całą partycję LittleFS (Formatowanie)
void wipeFilesystem();

// Współdzielona kolejka dla logowania
extern QueueHandle_t logQueue;

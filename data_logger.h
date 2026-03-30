#pragma once
#include <Arduino.h>

// System logowania i kolejkowania danych
struct TelemetryData {
  uint32_t timestamp;
  int rpm;
  int speed;
  int temp;
  int load;
  float volt;
  int iat;
  int tps;
  int map;
  int fuel;
};

// Inicjalizacja systemu plików i start nowego logu
void initDataLogger();

// Czyści stary arkusz i inicjuje nagłówki CSV dla nowego nagrania
void startDataLog();

// Zamyka cykl zapisu bezpiecznika isLogging
void stopDataLog();

// Zadanie asynchronicznego zapisu danych (FreeRTOS)
void taskLogging(void *pvParameters);

// Współdzielona kolejka dla logowania
extern QueueHandle_t logQueue;

// Zapisuje jeden wiersz danych do pliku CSV (wywoływane przez taskLogging)
void writeTelemetryToFlash(const TelemetryData& data);

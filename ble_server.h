#pragma once

// Inicjalizacja serwera Bluetooth Low Energy (BLE)
void setupBLEServer();

// Wysyłanie ramki z telemetrią do przeglądarki
void sendBLETelemetry();

// Struktura pakietu binarnego (Binary Protocol) — łącznie 30 bajtów.
struct __attribute__((packed)) TelemetryPacket {
  uint8_t magic = 0xBB;    // Prefiks rozpoznawczy dla pakietu binarnego
  uint8_t mode;           // DeviceMode (0-5)
  uint8_t speed;          // Prędkość km/h
  int16_t temp;           // Temp silnika
  uint8_t load;           // Load %
  float volt;             // Napięcie V (4 bajty)
  uint16_t rpm;           // Obroty RPM
  uint32_t timerResult;   // Wynik 0-100 ms
  uint32_t currentTime;   // Aktualny czas dragu ms
  int8_t iat;             // Temp dolotu
  uint8_t tps;            // Przepustnica %
  uint16_t map;           // Ciśnienie kPa
  uint8_t fuel;           // Paliwo %
  float gforce;           // Siła G (4 bajty)
  uint8_t logging;        // Flaga zapisu (0/1)
  uint8_t hz;             // Częstotliwość OBD (Hz)
};

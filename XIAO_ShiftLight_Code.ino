#include <Arduino.h>
#include <Preferences.h>
#include <FastLED.h>

#include "config.h"
#include "led_controller.h"
#include "can_bus.h"
#include "core_tasks.h"
#include "ble_server.h"
#include "data_logger.h"

// Definicje zmiennych globalnych (zadeklarowanych extern w config.h)
volatile int shiftLimit;
volatile int brightness;
volatile bool ecoMode;
volatile bool buzzerEnabled = true;

// Definicje flag i liczników czasu BLE
volatile bool bleConnected = false;
volatile uint32_t lastBLEActivity = 0;

volatile uint32_t lastRPMTime = 0; // Inicjalizacja czasu ostatniej poprawnej ramki

volatile int currentRPM = 0;
volatile int currentSpeed = 0;
volatile int currentTemp = 999; 
volatile int currentLoad = 0;
volatile float currentVolt = 12.0;
volatile int currentIAT = 999;
volatile int currentTPS = 0;
volatile int currentMAP = 0;
volatile int currentFuel = 0;
volatile float currentGForce = 0;

volatile DeviceMode currentMode = MODE_WELCOME;
volatile uint32_t dragTimerStart = 0;
volatile uint32_t timerResult = 0;

volatile bool isLogging = false; 
volatile uint32_t baseUsedBytes = 0;
volatile uint32_t currentFileSize = 0;
SemaphoreHandle_t fsMutex = NULL;

Preferences preferences;

void setup() {
  fsMutex = xSemaphoreCreateMutex();
  //Serial.begin(115200); // Odkomentuj tylko do debugowania przez USB

  lastRPMTime = millis(); // Punkt zerowy dla fail-safe, aby uniknąć błędu na starcie

  // Inicjalizacja LED (z led_controller.cpp)
  setupLEDs();

  // Inicjalizacja systemu plików Flash (LittleFS) dla Data Loggera
  initDataLogger();

  // Odczytujemy zapisane ustawienia, by system pamiętał je nawet po wyłączeniu zapłonu
  preferences.begin("shiftlight", true); // Parametr 'true' otwiera NVS w trybie "tylko do odczytu", co jest szybsze i bezpieczniejsze
  shiftLimit = preferences.getInt("rpm", DEFAULT_SHIFT_LIMIT);
  brightness = preferences.getInt("bright", DEFAULT_BRIGHTNESS);
  ecoMode = preferences.getBool("eco", false);
  buzzerEnabled = preferences.getBool("buzzer", true);
  preferences.end();
  
  // Ustawienie jasności matrycy LED (z led_controller.cpp)
  FastLED.setBrightness(brightness);

  // Core 0 (Protocol CPU): Bezkompromisowe nasłuchiwanie sprzętowe CAN Bus'a i komunikacja BLE
  xTaskCreatePinnedToCore(taskCore0, "Core0Task", 10000, NULL, 5, NULL, 0); // 10KB stosu, które wystarczy na obsługę BLE, CAN i logowania danych
  
  // Core 1 (Application CPU): Czysta dedykacja obliczeniowa pod silnik renderujący układu FastLED
  xTaskCreatePinnedToCore(taskCore1, "Core1Task", 10000, NULL, 1, NULL, 1); // 10KB stosu, które wystarczy na animacje FastLED
}

void loop() {
  // Architektura jest wielowątkowa, więc pusta pętla loop() nie jest potrzebna
  vTaskDelete(NULL);
}
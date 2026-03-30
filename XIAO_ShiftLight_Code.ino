#include <Arduino.h>
#include <Preferences.h>
#include <FastLED.h>

#include "config.h"
#include "led_controller.h"
#include "can_bus.h"
#include "core_tasks.h"
#include "web_server.h"
#include "data_logger.h"

// --- Definicje zmiennych globalnych (zadeklarowanych extern w config.h) ---
int shiftLimit;
int brightness;
bool ecoMode;
bool buzzerEnabled = true;
volatile uint32_t lastWebPing = 0;

volatile int currentRPM = 0;
volatile int currentSpeed = 0;
volatile int currentTemp = 999; 
volatile int currentLoad = 0;
volatile float currentVolt = 12.0;
volatile int currentIAT = 999;
volatile int currentTPS = 0;
volatile int currentMAP = 0;
volatile int currentFuel = 0;

volatile DeviceMode currentMode = MODE_SHIFT_LIGHT;
volatile uint32_t dragTimerStart = 0;
volatile uint32_t timerResult = 0;

volatile bool isLogging = false; // Definicja bezpieczeństwa zapisu na Dysk

Preferences preferences;

void setup() {
  Serial.begin(115200); // Komunikacja z komputerem przez USB do debugowania

  // Inicjalizacja LED (z led_controller.cpp)
  setupLEDs();

  // Odczytujemy zapisane ustawienia, by system pamiętał je nawet po wyłączeniu zapłonu
  preferences.begin("shiftlight", true); // Parametr 'true' otwiera NVS w trybie "tylko do odczytu", co jest szybsze i bezpieczniejsze
  shiftLimit = preferences.getInt("rpm", DEFAULT_SHIFT_LIMIT);
  brightness = preferences.getInt("bright", DEFAULT_BRIGHTNESS);
  ecoMode = preferences.getBool("eco", false);
  buzzerEnabled = preferences.getBool("buzzer", true);
  preferences.end();
  
  // Ustawienie jasności matrycy LED (z led_controller.cpp)
  FastLED.setBrightness(brightness);
  

  // Core 0 (Protocol CPU): Bezkompromisowe nasłuchiwanie sprzętowe CAN Bus'a i Web Serwera WiFi
  xTaskCreatePinnedToCore(taskCore0, "Core0Task", 10000, NULL, 1, NULL, 0); // 10KB stosu, które wystarczy na obsługę WiFi, CAN i logowania danych
  
  // Core 1 (Application CPU): Czysta dedykacja obliczeniowa pod silnik renderujący układu FastLED
  xTaskCreatePinnedToCore(taskCore1, "Core1Task", 10000, NULL, 1, NULL, 1); // 10KB stosu, które wystarczy na animacje FastLED
}

void loop() {
  // Architektura jest wielowątkowa, więc pusta pętla loop() nie jest potrzebna
  vTaskDelete(NULL); 
}
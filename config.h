#pragma once
#include <Arduino.h>

// Konfiguracja pinów oraz zmiennych globalnych
#define LED_PIN     D1      // Pin danych wychodzących do paska WS2812B
#define BUZZER_PIN  D2      // Pin sygnałowy buzzera
#define RX_PIN      D5      // CAN Receiver (Wewnętrzny kontroler TWAI ESP32-S3)
#define TX_PIN      D4      // CAN Transmitter (Wewnętrzny kontroler TWAI ESP32-S3)
#define NUM_LEDS    8       // Ilość używanych fizycznie diod RGB

// Konfiguracja CAN
#define CAN_BAUDRATE_500K   true    // Powrót na 500kbps (standard OBD2)
#define CAN_AUTO_RECOVERY   true    // Automatycznie odblokuj BUS OFF
#define CAN_TEST_LOOPBACK   false   // Ustaw na true, żeby sprawdzić sam moduł (bez auta)

// Ustawienia domyślne (Przed odczytem z pamięci rozerzalnej NVS)
#define DEFAULT_SHIFT_LIMIT 6000
#define DEFAULT_BRIGHTNESS 50

// Tryby pracy urządzenia
enum DeviceMode { 
  MODE_WELCOME = 0,
  MODE_SHIFT_LIGHT = 1, 
  MODE_0_100_COUNTDOWN = 2, 
  MODE_0_100_WAITING_FOR_LAUNCH = 3, 
  MODE_0_100_MEASURING = 4, 
  MODE_0_100_DONE = 5,
  MODE_LED_TEST = 6
};

// Zmienne konfiguracyjne z Flash (extern)
extern volatile int shiftLimit;
extern volatile int brightness;
extern volatile bool ecoMode;
extern volatile bool buzzerEnabled;

// Flagi i liczniki aktywności dla interfejsu Bluetooth
extern volatile bool bleConnected;
extern volatile uint32_t lastBLEActivity;

// Definicje unikalnych adresów UUID dla usługi i charakterystyk BLE
#define BLE_SERVICE_UUID   "4fafc201-1fb5-459e-8bcc-c5c9c331914b"
#define BLE_TX_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_RX_CHAR_UUID   "beb5483e-36e2-4688-b7f5-ea07361b26a8"

/* Zmienne współdzielone między rdzeniami w RTOS
   "volatile" informuje kompilator, aby nie optymalizował tych zmiennych do rejestru operacyjnego CPU,
   co zapobiega tzw. "stale data" w architekturze wielowątkowej. */
extern volatile int currentRPM;
extern volatile int currentSpeed;
extern volatile int currentTemp;     // Wartość 999 oznacza oczekiwanie na pierwszą poprawną paczkę danych z silnika (ECU)
extern volatile int currentLoad;     
extern volatile float currentVolt;
extern volatile int currentIAT;      // Temperatura powietrza dolotowego (Intake)
extern volatile int currentTPS;      // Pozycja przepustnicy (Throttle)
extern volatile int currentMAP;      // Ciśnienie w dolocie (Manifold Pressure)
extern volatile int currentFuel;     // Poziom paliwa w baku
extern volatile float currentGForce; // Siła G (przyspieszenie podłużne, liczone z delty prędkości)

// Status timera i pracy
extern volatile DeviceMode currentMode;
extern volatile uint32_t dragTimerStart;
extern volatile uint32_t timerResult;
extern volatile bool isLogging;
extern volatile uint32_t lastRPMTime; // Czas ostatniej poprawnej aktualizacji RPM
extern volatile uint8_t obdHz;        // Częstotliwość OBD (Hz)

// Mutex dla bezpiecznego dostępu do systemu plików (Flash) z obu rdzeni naraz
extern SemaphoreHandle_t fsMutex;
extern volatile uint32_t currentFileSize;
extern volatile uint32_t sessionStartFileSize;
extern volatile uint32_t sessionStartUsedBytes;

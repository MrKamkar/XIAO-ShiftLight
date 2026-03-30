#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/twai.h>
#include "config.h"
#include "core_tasks.h"
#include "can_bus.h"
#include "web_server.h"
#include "data_logger.h"
#include "led_controller.h"
#include "obd_pids.h"

void taskCore0(void *pvParameters) {
  // Inicjalizacja WiFi jako Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Kamcore_ShiftLight", "kamcore_pass"); // Nazwa sieci i hasło do połączenia
  esp_wifi_set_ps(WIFI_PS_NONE); // Wyłącza tryb oszczędzania energii WiFi
  
  // Inicjalizacja serwera WWW (z web_server.cpp)
  setupWebServer();

  // Inicjalizacja sterownika CAN (TWAI) ze standardem ISO 15765-4
#if CAN_TEST_LOOPBACK
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NO_ACK);
#else
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
#endif
#if CAN_BAUDRATE_500K
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#else
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
#endif
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Instalacja i uruchomienie sterownika TWAI (z walidacją)
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK || twai_start() != ESP_OK) {
    // Krytyczny błąd sprzętowy — zatrzymaj rdzeń 0
    while(true) vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // Konfiguracja alertów TWAI (event-driven zamiast polling)
  twai_reconfigure_alerts(TWAI_ALERT_RX_DATA | TWAI_ALERT_BUS_OFF, NULL);

  // Inicjalizacja Kolejki i Zadania Logowania (Rdzeń 0, niski priorytet)
  logQueue = xQueueCreate(50, sizeof(TelemetryData));
  xTaskCreatePinnedToCore(taskLogging, "TaskLogging", 8192, NULL, 1, NULL, 0);

  // Zmienne do zarządzania czasem zapytań CAN oraz zapisu telemetrii
  uint32_t lastOBDRequest = 0;
  uint32_t lastTempRequest = 0;
  uint32_t lastLogTime = 0;
  int obdSeq = 0;
  bool waitingForResponse = false; // Flaga oczekiwania na odpowiedź z ECU

  while (true) {
    // Serwer musi działać tu, ponieważ jest on obsługiwany przez rdzeń 0
    handleWebServerClient();
    
    // Logika wysyłania danych do kolejki (co 100ms)
    if (isLogging && (millis() - lastLogTime >= 100)) {
      TelemetryData data = {
        millis(), currentRPM, currentSpeed, currentTemp, currentLoad, 
        currentVolt, currentIAT, currentTPS, currentMAP, currentFuel
      };
      // Wysyłamy do kolejki; jeśli pełna (np. Flash zajęty), odrzucamy paczkę
      xQueueSend(logQueue, &data, 0); 
      lastLogTime = millis();
    }

    // Event-driven odbiór CAN (bez blokowania pętli)
    uint32_t alerts = 0;
    twai_read_alerts(&alerts, 0); // Non-blocking: sprawdza flagi zdarzeń TWAI

    // Odbiór danych z magistrali CAN (wyzwalany alertem RX_DATA)
    if (alerts & TWAI_ALERT_RX_DATA) {
      twai_message_t rx_msg;
      while (twai_receive(&rx_msg, 0) == ESP_OK) {
        // Sprawdź czy odpowiedź pochodzi z zakresu adresów diagnostycznych ECU (0x7E8 - 0x7EF)
        if (rx_msg.identifier >= CAN_ID_OBD_REPLY_MIN && rx_msg.identifier <= CAN_ID_OBD_REPLY_MAX) {
          
          // Sprawdź czy bajt mode odpowiada zapytaniu o dane bieżące (0x01 + 0x40 = 0x41)
          if (rx_msg.data[1] == (OBD_MODE_CURRENT_DATA | OBD_REPLY_OFFSET)) { 
            
            // Otrzymaliśmy poprawną odpowiedź OBD, więc czyścimy flagę oczekiwania
            waitingForResponse = false; 

            switch (rx_msg.data[2]) {
              case PID_ENGINE_RPM:
                currentRPM = ((rx_msg.data[3] * 256) + rx_msg.data[4]) / 4;
                lastRPMTime = millis(); // Odświeżenie znacznika fail-safe
                break;
              case PID_VEHICLE_SPEED:
                currentSpeed = rx_msg.data[3];

                // Logika Drag Timera 0-100 km/h
                if (currentMode == MODE_0_100_WAITING_FOR_LAUNCH && currentSpeed > 0) {
                  dragTimerStart = millis();
                  currentMode = MODE_0_100_MEASURING;
                } else if (currentMode == MODE_0_100_MEASURING && currentSpeed >= 100) {
                  timerResult = millis() - dragTimerStart;
                  currentMode = MODE_0_100_DONE;
                }
                break;
                
              // Parametry wolnozmienne
              case PID_COOLANT_TEMP: currentTemp = rx_msg.data[3] - 40; break;
              case PID_ENGINE_LOAD:  currentLoad = rx_msg.data[3] * 100 / 255; break;
              case PID_BATTERY_VOLT: currentVolt = ((rx_msg.data[3] * 256) + rx_msg.data[4]) / 1000.0; break;
              case PID_IAT_TEMP:     currentIAT = rx_msg.data[3] - 40; break;
              case PID_THROTTLE_POS: currentTPS = rx_msg.data[3] * 100 / 255; break;
              case PID_MAP_PRESSURE: currentMAP = rx_msg.data[3]; break;
              case PID_FUEL_LEVEL:   currentFuel = rx_msg.data[3] * 100 / 255; break;
            }
          }
        }
      }
    }

    // Natychmiastowy auto-recovery CAN BUS OFF (event-driven zamiast pollingu co 2s)
    #if CAN_AUTO_RECOVERY
    if (alerts & TWAI_ALERT_BUS_OFF) {
      twai_initiate_recovery();
    }
    #endif
    
    // Agile Polling Schedule: 15ms priorytet RPM
    uint32_t now = millis();
    bool pollTimeout = (now - lastOBDRequest >= 50);

    if (!waitingForResponse || pollTimeout) {
      if (now - lastOBDRequest >= 15) { 
        uint8_t pidRequest = PID_ENGINE_RPM; 
        bool webActive = (now - lastWebPing < 3000);
        
        // 1. Zarządzanie parametrami wolnozmiennymi (wyzwalane czasowo)
        if (now - lastTempRequest >= (webActive ? 500 : 5000)) { 
          static int slowSeq = 0;
          // Rotacja parametrów dodatkowych, aby nie zapchać magistrali
          switch (slowSeq) {
            case 0: pidRequest = PID_COOLANT_TEMP; break; // Coolant zawsze czytamy, nawet bez aktywnego WWW
            case 1: pidRequest = webActive ? PID_ENGINE_LOAD : PID_ENGINE_RPM; break;
            case 2: pidRequest = webActive ? PID_BATTERY_VOLT : PID_ENGINE_RPM; break;
            case 3: pidRequest = webActive ? PID_IAT_TEMP : PID_ENGINE_RPM; break;
            case 4: pidRequest = webActive ? PID_MAP_PRESSURE : PID_ENGINE_RPM; break;
            case 5: pidRequest = webActive ? PID_FUEL_LEVEL : PID_ENGINE_RPM; break;
            default: pidRequest = PID_COOLANT_TEMP; break;
          }
        
          slowSeq = (slowSeq + 1) % 6;
          lastTempRequest = now;
        } 
        else {
          // 2. Logika dla parametrów szybkozmiennych (zależna od trybu urządzenia)
          if (currentMode == MODE_0_100_MEASURING || currentMode == MODE_0_100_WAITING_FOR_LAUNCH) {
            // Podczas pomiaru 0-100: przeplatamy RPM i Prędkość (50/50)
            obdSeq = (obdSeq + 1) % 2;
            pidRequest = (obdSeq == 0) ? PID_ENGINE_RPM : PID_VEHICLE_SPEED; 
          }
          else if (!webActive) { 
            // Tryb ECO: Nikt nie patrzy na stronę -> pytamy TYLKO o obroty dla LEDów
            pidRequest = PID_ENGINE_RPM; 
          } 
          else {
            // Tryb FULL: Ktoś patrzy na zegary -> przeplatamy RPM, Speed i Przepustnicę
            obdSeq = (obdSeq + 1) % 4;
            if (obdSeq == 0 || obdSeq == 2) pidRequest = PID_ENGINE_RPM;
            else if (obdSeq == 1) pidRequest = PID_VEHICLE_SPEED;
            else pidRequest = PID_THROTTLE_POS;
          }
        }
        
        sendOBDRequest(pidRequest);
        lastOBDRequest = now;
        waitingForResponse = true; // Uzbrajamy oczekiwanie na nową ramkę
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// Rdzeń 1 (Application CPU): Wyświetlanie diod LED i animacje
void taskCore1(void *pvParameters) {
  while (true) {
    updateLEDs(); 
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

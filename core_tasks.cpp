#include <Arduino.h>
#include <WiFi.h>
#include <driver/twai.h>
#include "config.h"
#include "core_tasks.h"
#include "can_bus.h"
#include "web_server.h"
#include "data_logger.h"
#include "obd_pids.h"

void taskCore0(void *pvParameters) {
  // Inicjalizacja WiFi jako Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Kamcore_ShiftLight", "kamcore_drive"); // Nazwa sieci i hasło do połączenia
  
  // Inicjalizacja serwera WWW (z web_server.cpp)
  setupWebServer();

  // Inicjalizacja sterownika CAN (TWAI) ze standardem ISO 15765-4
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Instalacja i uruchomienie sterownika TWAI
  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  
  // Zmienne do zarządzania czasem zapytań CAN oraz zapisu telemetrii
  uint32_t lastOBDRequest = millis();
  uint32_t lastTempRequest = 0;
  uint32_t lastLogTime = 0;
  int obdSeq = 0;

  while (true) {
    // Serwer musi działać tu, ponieważ jest on obsługiwany przez rdzeń 0
    handleWebServerClient();
    
    // Zapis do pamięci Flash co 100ms
    if (isLogging) {
      if (millis() - lastLogTime >= 100) {
        logTelemetryData();
        lastLogTime = millis();
      }
    }

    // Odbiór danych z magistrali CAN
    twai_message_t rx_msg;
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
      do {
        // Sprawdź czy odpowiedź pochodzi z zakresu adresów diagnostycznych ECU (0x7E8 - 0x7EF)
        if (rx_msg.identifier >= CAN_ID_OBD_REPLY_MIN && rx_msg.identifier <= CAN_ID_OBD_REPLY_MAX) {
          
          // Sprawdź czy bajt Mode odpowiada zapytaniu o dane bieżące (0x01 + 0x40 = 0x41)
          if (rx_msg.data[1] == (OBD_MODE_CURRENT_DATA | OBD_REPLY_OFFSET)) { 
            
            // Decyduj o przypisaniu wartości na podstawie PID-u zawartego w trzecim bajcie
            switch (rx_msg.data[2]) {
              case PID_ENGINE_RPM:
                currentRPM = ((rx_msg.data[3] * 256) + rx_msg.data[4]) / 4;
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
      } while (twai_receive(&rx_msg, 0) == ESP_OK); // Przetwarzanie całego bufora RX w jednym cyklu (Zero-Latency Synchronization)
    }
    
    // Dynamiczne zarządzanie priorytetami zapytań CAN dla płynności diod
    if (millis() - lastOBDRequest >= 20) {
      uint8_t pidRequest = PID_ENGINE_RPM; // Domyślnie zawsze pytamy o obroty
      bool webActive = (millis() - lastWebPing < 3000); // Czy w ciągu ostatnich 3s ktoś zaglądał na stronę?
      
      // Zarządzanie parametrami wolnozmiennymi
      if (millis() - lastTempRequest >= (webActive ? 250 : 3000)) { 
        static int slowSeq = 0;
        // Rotacja parametrów dodatkowych, aby nie zapchać magistrali
        switch (slowSeq) {
          case 0: pidRequest = PID_COOLANT_TEMP; break;
          case 1: if (webActive) pidRequest = PID_ENGINE_LOAD; break;
          case 2: if (webActive) pidRequest = PID_BATTERY_VOLT; break;
          case 3: if (webActive) pidRequest = PID_IAT_TEMP; break;
          case 4: if (webActive) pidRequest = PID_MAP_PRESSURE; break;
          case 5: if (webActive) pidRequest = PID_FUEL_LEVEL; break;
          default: pidRequest = PID_COOLANT_TEMP; break;
        }
        
        slowSeq = (slowSeq + 1) % 6;
        lastTempRequest = millis(); // Reset timera dla wolnej kolejki
      } else {
        // Logika dla parametrów szybkozmiennych (RPM / Speed / TPS)
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
      
      sendOBDRequest(pidRequest); // Wysyłanie zapytania CAN
      lastOBDRequest = millis(); // Reset timera względem poprzedniego zapytania CAN
    }
  }
}

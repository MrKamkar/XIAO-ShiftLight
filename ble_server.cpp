#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <FastLED.h>
#include "config.h"
#include "ble_server.h"
#include "data_logger.h"

extern Preferences preferences;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
BLECharacteristic *pRxCharacteristic = NULL;

// Wyzwalane przy parowaniu/rozłączeniu
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    lastBLEActivity = millis();
    // Odświeżamy timer, żeby nie zablokował CAN busa
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    // Ponownie aktywujemy rozgłaszanie, by urządzenie było znów widoczne
    pServer->startAdvertising();
  }
};

// Wyzwalane gdy strona internetowa wyśle ciąg znaków
class MyRxCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String cmd = pCharacteristic->getValue();
    
    if (cmd.length() > 0) {
      cmd.trim(); // Usunięcie \n, \r, itp.
      
      lastBLEActivity = millis(); // Odświeżenie znacznika aktywności

      if (cmd.startsWith("cmd:save:")) {
        // Parsowanie stringa cmd:save:6000:50:1:1
        int s1 = cmd.indexOf(':', 9);
        int s2 = cmd.indexOf(':', s1+1);
        int s3 = cmd.indexOf(':', s2+1);
        
        if (s1 > 0 && s2 > 0 && s3 > 0) {
          shiftLimit = cmd.substring(9, s1).toInt();
          brightness = cmd.substring(s1+1, s2).toInt();
          ecoMode = (cmd.substring(s2+1, s3).toInt() == 1);
          buzzerEnabled = (cmd.substring(s3+1).toInt() == 1);
          
          preferences.begin("shiftlight", false);
          preferences.putInt("rpm", shiftLimit);
          preferences.putInt("bright", brightness);
          preferences.putBool("eco", ecoMode);
          preferences.putBool("buzzer", buzzerEnabled);
          preferences.end();
          
          FastLED.setBrightness(brightness);
        }
      } 
      else if (cmd == "cmd:start0100") {
        currentMode = MODE_0_100_COUNTDOWN;
      } 
      else if (cmd == "cmd:cancel0100") {
        currentMode = MODE_SHIFT_LIGHT;
      } 
      else if (cmd == "cmd:log_start") {
        startDataLog();
      } 
      else if (cmd == "cmd:log_stop") {
        stopDataLog();
      } 
      else if (cmd == "cmd:req_conf") {
        // Aplikacja (app.js) zażądała wgrania aktualnej konfiguracji z ESP32 tuż po połączeniu.
        char confBuf[128];
        snprintf(confBuf, sizeof(confBuf), "{\"type\":\"config\",\"rpm\":%d,\"bright\":%d,\"eco\":%s,\"buzzer\":%s}\n",
                 shiftLimit, brightness, ecoMode ? "true" : "false", buzzerEnabled ? "true" : "false");
        pTxCharacteristic->setValue((uint8_t*)confBuf, strlen(confBuf));
        pTxCharacteristic->notify();
      }
    }
  }
};

void setupBLEServer() {
  BLEDevice::init("XIAO_ShiftLight");
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);

  // Charakterystyka TX (Notify - stąd dane płyną na zewnątrz)
  pTxCharacteristic = pService->createCharacteristic(
                        BLE_TX_CHAR_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902()); // Niezbędny by telefony radziły sobie z Notification

  // Charakterystyka RX (Write - tędy przeglądarka wysyła komendy do nas)
  pRxCharacteristic = pService->createCharacteristic(
                         BLE_RX_CHAR_UUID,
                         BLECharacteristic::PROPERTY_WRITE
                       );
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();
  
  // Sprawienie, że urządzenie będzie widoczne przy skanowaniu.
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // Minimalizacja interwału dla iOS
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

// Funkcja wywoływana cyklicznie przez rdzeń 0, wysyłająca paczkę JSON przez BLE
void sendBLETelemetry() {
  if (!bleConnected || pTxCharacteristic == NULL) return;

  // Użycie statycznego bufora
  static char jsonBuffer[380];
  uint32_t cTime = (currentMode == MODE_0_100_MEASURING) ? (millis() - dragTimerStart) : 0;
  
  int len = snprintf(jsonBuffer, sizeof(jsonBuffer),
           "{\"mode\":%d,\"speed\":%d,\"temp\":%d,\"load\":%d,\"volt\":%.1f,\"rpm\":%d,\"time\":%lu,\"current_time\":%lu,\"iat\":%d,\"tps\":%d,\"map\":%d,\"fuel\":%d,\"gforce\":%.2f,\"log\":%s}\n",
           currentMode, currentSpeed, currentTemp, currentLoad, currentVolt, currentRPM, timerResult, cTime, currentIAT, currentTPS, currentMAP, currentFuel, currentGForce, isLogging ? "true" : "false");
           
  if (len > 0) {
    pTxCharacteristic->setValue((uint8_t*)jsonBuffer, len);
    pTxCharacteristic->notify();
  }
}
